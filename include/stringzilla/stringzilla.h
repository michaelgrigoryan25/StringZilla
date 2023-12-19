/**
 *  @brief  StringZilla is a collection of fast string algorithms, designed to be used in Big Data applications.
 *
 *  @section   Compatibility with LibC and STL
 *
 *  The C++ Standard Templates Library provides an `std::string` and `std::string_view` classes with similar
 *  functionality. LibC, in turn, provides the "string.h" header with a set of functions for working with C strings.
 *  Both of those have a fairly constrained interface, as well as poor utilization of SIMD and SWAR techniques.
 *  StringZilla improves on both of those, by providing a more flexible interface, and better performance.
 *  If you are well familiar use the following index to find the equivalent functionality:
 *
 *      - void    *memchr(const void *, int, size_t); -> sz_find_byte
 *      - int      memcmp(const void *, const void *, size_t); -> sz_order, sz_equal
 *
 *      - char    *strchr(const char *, int); -> sz_find_byte
 *      - int      strcmp(const char *, const char *); -> sz_order, sz_equal
 *      - size_t   strcspn(const char *, const char *); -> sz_prefix_rejected
 *      - size_t   strlen(const char *);-> sz_find_byte
 *      - size_t   strspn(const char *, const char *); -> sz_prefix_accepted
 *      - char    *strstr(const char *, const char *); -> sz_find
 *
 *      - void    *memccpy(void *restrict, const void *restrict, int, size_t);
 *      - void    *memcpy(void *restrict, const void *restrict, size_t);
 *      - void    *memmove(void *, const void *, size_t);
 *      - void    *memset(void *, int, size_t);
 *      - char    *strcat(char *restrict, const char *restrict);
 *      - int      strcoll(const char *, const char *);
 *      - char    *strcpy(char *restrict, const char *restrict);
 *      - char    *strdup(const char *);
 *      - char    *strerror(int);
 *      - int     *strerror_r(int, char *, size_t);
 *      - char    *strncat(char *restrict, const char *restrict, size_t);
 *      - int      strncmp(const char *, const char *, size_t);
 *      - char    *strncpy(char *restrict, const char *restrict, size_t);
 *      - char    *strpbrk(const char *, const char *);
 *      - char    *strrchr(const char *, int);
 *      - char    *strtok(char *restrict, const char *restrict);
 *      - char    *strtok_r(char *, const char *, char **);
 *      - size_t   strxfrm(char *restrict, const char *restrict, size_t);
 *
 *  LibC documentation: https://pubs.opengroup.org/onlinepubs/009695399/basedefs/string.h.html
 *  STL documentation: https://en.cppreference.com/w/cpp/header/string_view
 *
 *
 */

#ifndef STRINGZILLA_H_
#define STRINGZILLA_H_

/**
 *  @brief  Annotation for the public API symbols.
 */
#if defined(_WIN32) || defined(__CYGWIN__)
#define SZ_PUBLIC __declspec(dllexport)
#elif __GNUC__ >= 4
#define SZ_PUBLIC __attribute__((visibility("default")))
#else
#define SZ_PUBLIC
#endif
#define SZ_INTERNAL inline static

/**
 *  @brief  Generally `NULL` is coming from locale.h, stddef.h, stdio.h, stdlib.h, string.h, time.h,
 *          and wchar.h, according to the C standard.
 */
#ifndef NULL
#define NULL ((void *)0)
#endif

/**
 *  @brief  Generally `CHAR_BIT` is coming from limits.h, according to the C standard.
 */
#ifndef CHAR_BIT
#define CHAR_BIT (8)
#endif

/**
 *  @brief  Compile-time assert macro similar to `static_assert` in C++.
 */
#define SZ_STATIC_ASSERT(condition, name)                \
    typedef struct {                                     \
        int static_assert_##name : (condition) ? 1 : -1; \
    } sz_static_assert_##name##_t

/**
 *  @brief  A misaligned load can be - trying to fetch eight consecutive bytes from an address
 *          that is not divisble by eight.
 *
 *  Most platforms support it, but there is no industry standard way to check for those.
 *  This value will mostly affect the performance of the serial (SWAR) backend.
 */
#ifndef SZ_USE_MISALIGNED_LOADS
#define SZ_USE_MISALIGNED_LOADS 1
#endif

/*
 *  Hardware feature detection.
 */
#ifndef SZ_USE_X86_AVX512
#ifdef __AVX512BW__
#define SZ_USE_X86_AVX512 1
#else
#define SZ_USE_X86_AVX512 0
#endif
#endif

#ifndef SZ_USE_X86_AVX2
#ifdef __AVX2__
#define SZ_USE_X86_AVX2 1
#else
#define SZ_USE_X86_AVX2 0
#endif
#endif

#ifndef SZ_USE_X86_SSE42
#ifdef __SSE4_2__
#define SZ_USE_X86_SSE42 1
#else
#define SZ_USE_X86_SSE42 0
#endif
#endif

#ifndef SZ_USE_ARM_NEON
#ifdef __ARM_NEON
#define SZ_USE_ARM_NEON 1
#else
#define SZ_USE_ARM_NEON 0
#endif
#endif

#ifndef SZ_USE_ARM_CRC32
#ifdef __ARM_FEATURE_CRC32
#define SZ_USE_ARM_CRC32 1
#else
#define SZ_USE_ARM_CRC32 0
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 *  @brief  Analogous to `size_t` and `std::size_t`, unsigned integer, identical to pointer size.
 *          64-bit on most platforms where pointers are 64-bit.
 *          32-bit on platforms where pointers are 32-bit.
 */
#if defined(__LP64__) || defined(_LP64) || defined(__x86_64__) || defined(_WIN64)
typedef unsigned long long sz_size_t;
typedef long long sz_ssize_t;
#else
typedef unsigned sz_size_t;
typedef unsigned sz_ssize_t;
#endif
SZ_STATIC_ASSERT(sizeof(sz_size_t) == sizeof(void *), sz_size_t_must_be_pointer_size);
SZ_STATIC_ASSERT(sizeof(sz_ssize_t) == sizeof(void *), sz_ssize_t_must_be_pointer_size);

typedef unsigned char sz_u8_t;       /// Always 8 bits
typedef unsigned short sz_u16_t;     /// Always 16 bits
typedef int sz_i32_t;                /// Always 32 bits
typedef unsigned int sz_u32_t;       /// Always 32 bits
typedef unsigned long long sz_u64_t; /// Always 64 bits

typedef char *sz_ptr_t;        /// A type alias for `char *`
typedef char const *sz_cptr_t; /// A type alias for `char const *`
typedef char sz_error_cost_t;  /// Character mismatch cost for fuzzy matching functions

typedef enum { sz_false_k = 0, sz_true_k = 1 } sz_bool_t;                        /// Only one relevant bit
typedef enum { sz_less_k = -1, sz_equal_k = 0, sz_greater_k = 1 } sz_ordering_t; /// Only three possible states: <=>

/**
 *  @brief  Computes the length of the NULL-termainted string. Equivalent to `strlen(a)` in LibC.
 *          Convenience method calling `sz_find_byte(text, 0)` under the hood.
 *
 *  @param text     String to enumerate.
 *  @return         Unsigned pointer-sized integer for the length of the string.
 */
SZ_PUBLIC sz_size_t sz_length_termainted(sz_cptr_t text);

/**
 *  @brief  Locates first matching substring. Equivalent to `strstr(haystack, needle)` in LibC.
 *          Convenience method, that relies on the `sz_length_termainted` and `sz_find`.
 *
 *  @param haystack Haystack - the string to search in.
 *  @param needle   Needle - substring to find.
 *  @return         Address of the first match.
 */
SZ_PUBLIC sz_cptr_t sz_find_terminated(sz_cptr_t haystack, sz_cptr_t needle);

/**
 *  @brief  Estimates the relative order of two NULL-terminated strings. Equivalent to `strcmp(a, b)` in LibC.
 *          Similar to calling `sz_length_termainted` and `sz_order`.
 *
 *  @param a        First null-terminated string to compare.
 *  @param b        Second null-terminated string to compare.
 *  @return         Negative if (a < b), positive if (a > b), zero if they are equal.
 */
SZ_PUBLIC sz_ordering_t sz_order_terminated(sz_cptr_t a, sz_cptr_t b);

/**
 *  @brief  Computes the hash of a string.
 *
 *  @section    Why not use CRC32?
 *
 *  Cyclic Redundancy Check 32 is one of the most commonly used hash functions in Computer Science.
 *  It has in-hardware support on both x86 and Arm, for both 8-bit, 16-bit, 32-bit, and 64-bit words.
 *  In case of Arm more than one polynomial is supported. It is, however, somewhat limiting for Big Data
 *  usecases, which often have to deal with more than 4 Billion strings, making collisions unavoidable.
 *  Moreover, the existing SIMD approaches are tricky, combining general purpose computations with
 *  specialized intstructions, to utilize more silicon in every cycle.
 *
 *  Some of the best articles on CRC32:
 *  - Comprehensive derivation of approaches: https://github.com/komrad36/CRC
 *  - Faster computation for 4 KB buffers on x86: https://www.corsix.org/content/fast-crc32c-4k
 *  - Comparing different lookup tables: https://create.stephan-brumme.com/crc32
 *
 *  Some of the best open-source implementations:
 *  - Peter Cawley: https://github.com/corsix/fast-crc32
 *  - Stephan Brumme: https://github.com/stbrumme/crc32
 *
 *  @section    Modern Algorithms
 *
 *  MurmurHash from 2008 by Austin Appleby is one of the best known non-cryptographic hashes.
 *  It has a very short implementation and is capable of producing 32-bit and 128-bit hashes.
 *  https://github.com/aappleby/smhasher/tree/61a0530f28277f2e850bfc39600ce61d02b518de
 *
 *  The CityHash from 2011 by Google and the xxHash improve on that, better leveraging
 *  the superscalar nature of modern CPUs and producing 64-bit and 128-bit hashes.
 *  https://opensource.googleblog.com/2011/04/introducing-cityhash
 *  https://github.com/Cyan4973/xxHash
 *
 *  Neither of those functions are cryptographic, unlike MD5, SHA, and BLAKE algorithms.
 *  Most of those are based on the Merkle–Damgård construction, and aren't resistant to
 *  the length-extension attacks. Current state of the Art, might be the BLAKE3 algorithm.
 *  It's resistant to a broad range of attacks, can process 2 bytes per CPU cycle, and comes
 *  with a very optimized official implementation for C and Rust. It has the same 128-bit
 *  security level as the BLAKE2, and achieves its performance gains by reducing the number
 *  of mixing rounds, and processing data in 1 KiB chunks, which is great for longer strings,
 *  but may result in poor performance on short ones.
 *  https://en.wikipedia.org/wiki/BLAKE_(hash_function)#BLAKE3
 *  https://github.com/BLAKE3-team/BLAKE3
 *
 *  As shown, choosing the right hashing algorithm for your application can be crucial from
 *  both performance and security standpoint. Assuming, this functionality will be mostly used on
 *  multi-word short UTF8 strings, StringZilla implements a very simple scheme derived from MurMur3.
 *
 *  @param text     String to hash.
 *  @param length   Number of bytes in the text.
 *  @return         32-bit hash value.
 */
SZ_PUBLIC sz_u64_t sz_hash(sz_cptr_t text, sz_size_t length);
SZ_PUBLIC sz_u64_t sz_hash_serial(sz_cptr_t text, sz_size_t length);
SZ_PUBLIC sz_u64_t sz_hash_avx512(sz_cptr_t text, sz_size_t length);
SZ_PUBLIC sz_u64_t sz_hash_neon(sz_cptr_t text, sz_size_t length);

typedef sz_u64_t (*sz_hash_t)(sz_cptr_t, sz_size_t);

/**
 *  @brief  Checks if two string are equal.
 *          Similar to `memcmp(a, b, length) == 0` in LibC and `a == b` in STL.
 *
 *  The implementation of this function is very similar to `sz_order`, but the usage patterns are different.
 *  This function is more often used in parsing, while `sz_order` is often used in sorting.
 *  It works best on platforms with cheap
 *
 *  @param a        First string to compare.
 *  @param b        Second string to compare.
 *  @param length   Number of bytes in both strings.
 *  @return         1 if strings match, 0 otherwise.
 */
SZ_PUBLIC sz_bool_t sz_equal(sz_cptr_t a, sz_cptr_t b, sz_size_t length);
SZ_PUBLIC sz_bool_t sz_equal_serial(sz_cptr_t a, sz_cptr_t b, sz_size_t length);
SZ_PUBLIC sz_bool_t sz_equal_avx512(sz_cptr_t a, sz_cptr_t b, sz_size_t length);

/**
 *  @brief  Estimates the relative order of two strings. Equivalent to `memcmp(a, b, length)` in LibC.
 *          Can be used on different length strings.
 *
 *  @param a        First string to compare.
 *  @param a_length Number of bytes in the first string.
 *  @param b        Second string to compare.
 *  @param b_length Number of bytes in the second string.
 *  @return         Negative if (a < b), positive if (a > b), zero if they are equal.
 */
SZ_PUBLIC sz_ordering_t sz_order(sz_cptr_t a, sz_size_t a_length, sz_cptr_t b, sz_size_t b_length);
SZ_PUBLIC sz_ordering_t sz_order_serial(sz_cptr_t a, sz_size_t a_length, sz_cptr_t b, sz_size_t b_length);
SZ_PUBLIC sz_ordering_t sz_order_avx512(sz_cptr_t a, sz_size_t a_length, sz_cptr_t b, sz_size_t b_length);

typedef sz_ordering_t (*sz_order_t)(sz_cptr_t, sz_size_t, sz_cptr_t, sz_size_t);

/**
 *  @brief  Locates first matching byte in a string. Equivalent to `memchr(haystack, *needle, h_length)` in LibC.
 *
 *  @param haystack Haystack - the string to search in.
 *  @param h_length Number of bytes in the haystack.
 *  @param needle   Needle - single-byte substring to find.
 *  @return         Address of the first match.
 */
SZ_PUBLIC sz_cptr_t sz_find_byte(sz_cptr_t haystack, sz_size_t h_length, sz_cptr_t needle);
SZ_PUBLIC sz_cptr_t sz_find_byte_serial(sz_cptr_t haystack, sz_size_t h_length, sz_cptr_t needle);
SZ_PUBLIC sz_cptr_t sz_find_byte_avx512(sz_cptr_t haystack, sz_size_t h_length, sz_cptr_t needle);

typedef sz_cptr_t (*sz_find_byte_t)(sz_cptr_t, sz_size_t, sz_cptr_t);

/**
 *  @brief  Locates first matching substring. Similar to `strstr(haystack, needle)` in LibC, but requires known length.
 *
 *  Uses different algorithms for different needle lengths and backends:
 *
 *  > Exact matching for 1-, 2-, 3-, and 4-character-long needles.
 *  > Bitap "Shift Or" (Baeza-Yates-Gonnet) algorithm for serial (SWAR) backend.
 *  > Two-way heuristic for longer needles with SIMD backends.
 *
 *  @section Reading Materials
 *
 *  Exact String Matching Algorithms in Java: https://www-igm.univ-mlv.fr/~lecroq/string/
 *  SIMD-friendly algorithms for substring searching: http://0x80.pl/articles/simd-strfind.html
 *
 *  @param haystack Haystack - the string to search in.
 *  @param h_length Number of bytes in the haystack.
 *  @param needle   Needle - substring to find.
 *  @param n_length Number of bytes in the needle.
 *  @return         Address of the first match.
 */
SZ_PUBLIC sz_cptr_t sz_find(sz_cptr_t haystack, sz_size_t h_length, sz_cptr_t needle, sz_size_t n_length);
SZ_PUBLIC sz_cptr_t sz_find_serial(sz_cptr_t haystack, sz_size_t h_length, sz_cptr_t needle, sz_size_t n_length);
SZ_PUBLIC sz_cptr_t sz_find_avx512(sz_cptr_t haystack, sz_size_t h_length, sz_cptr_t needle, sz_size_t n_length);
SZ_PUBLIC sz_cptr_t sz_find_avx2(sz_cptr_t haystack, sz_size_t h_length, sz_cptr_t needle, sz_size_t n_length);
SZ_PUBLIC sz_cptr_t sz_find_neon(sz_cptr_t haystack, sz_size_t h_length, sz_cptr_t needle, sz_size_t n_length);

SZ_PUBLIC sz_cptr_t sz_find_last(sz_cptr_t haystack, sz_size_t h_length, sz_cptr_t needle, sz_size_t n_length);
SZ_PUBLIC sz_cptr_t sz_find_last_serial(sz_cptr_t haystack, sz_size_t h_length, sz_cptr_t needle, sz_size_t n_length);

typedef sz_cptr_t (*sz_find_t)(sz_cptr_t, sz_size_t, sz_cptr_t, sz_size_t);

/**
 *  @brief  Enumerates matching character forming a prefix of given string.
 *          Equivalent to `strspn(text, accepted)` in LibC. Similar to `strcpan(text, rejected)`.
 *
 *  @param text     String to be trimmed.
 *  @param accepted Set of accepted characters.
 *  @return         Number of bytes forming the prefix.
 */
SZ_PUBLIC sz_size_t sz_prefix_accepted(sz_cptr_t text, sz_size_t length, sz_cptr_t accepted, sz_size_t count);
SZ_PUBLIC sz_size_t sz_prefix_accepted_serial(sz_cptr_t text, sz_size_t length, sz_cptr_t accepted, sz_size_t count);
SZ_PUBLIC sz_size_t sz_prefix_accepted_avx512(sz_cptr_t text, sz_size_t length, sz_cptr_t accepted, sz_size_t count);

typedef sz_cptr_t (*sz_prefix_accepted_t)(sz_cptr_t, sz_size_t, sz_cptr_t, sz_size_t);

/**
 *  @brief  Enumerates number non-matching character forming a prefix of given string.
 *          Equivalent to `strcspn(text, rejected)` in LibC. Similar to `strspn(text, accepted)`.
 *
 *  @param text     String to be trimmed.
 *  @param rejected Set of rejected characters.
 *  @return         Number of bytes forming the prefix.
 */
SZ_PUBLIC sz_size_t sz_prefix_rejected(sz_cptr_t text, sz_size_t length, sz_cptr_t rejected, sz_size_t count);
SZ_PUBLIC sz_size_t sz_prefix_rejected_serial(sz_cptr_t text, sz_size_t length, sz_cptr_t rejected, sz_size_t count);
SZ_PUBLIC sz_size_t sz_prefix_rejected_avx512(sz_cptr_t text, sz_size_t length, sz_cptr_t rejected, sz_size_t count);

typedef sz_cptr_t (*sz_prefix_rejected_t)(sz_cptr_t, sz_size_t, sz_cptr_t, sz_size_t);

/**
 *  @brief  Equivalent to `for (char & c : text) c = tolower(c)`.
 *
 *  ASCII characters [A, Z] map to decimals [65, 90], and [a, z] map to [97, 122].
 *  So there are 26 english letters, shifted by 32 values, meaning that a conversion
 *  can be done by flipping the 5th bit each inappropriate character byte. This, however,
 *  breaks for extended ASCII, so a different solution is needed.
 *  http://0x80.pl/notesen/2016-01-06-swar-swap-case.html
 *
 *  @param text     String to be normalized.
 *  @param length   Number of bytes in the string.
 *  @param result   Output string, can point to the same address as ::text.
 */
SZ_PUBLIC void sz_tolower(sz_cptr_t text, sz_size_t length, sz_ptr_t result);
SZ_PUBLIC void sz_tolower_serial(sz_cptr_t text, sz_size_t length, sz_ptr_t result);
SZ_PUBLIC void sz_tolower_avx512(sz_cptr_t text, sz_size_t length, sz_ptr_t result);

/**
 *  @brief  Equivalent to `for (char & c : text) c = toupper(c)`.
 *
 *  ASCII characters [A, Z] map to decimals [65, 90], and [a, z] map to [97, 122].
 *  So there are 26 english letters, shifted by 32 values, meaning that a conversion
 *  can be done by flipping the 5th bit each inappropriate character byte. This, however,
 *  breaks for extended ASCII, so a different solution is needed.
 *  http://0x80.pl/notesen/2016-01-06-swar-swap-case.html
 *
 *  @param text     String to be normalized.
 *  @param length   Number of bytes in the string.
 *  @param result   Output string, can point to the same address as ::text.
 */
SZ_PUBLIC void sz_toupper(sz_cptr_t text, sz_size_t length, sz_ptr_t result);
SZ_PUBLIC void sz_toupper_serial(sz_cptr_t text, sz_size_t length, sz_ptr_t result);
SZ_PUBLIC void sz_toupper_avx512(sz_cptr_t text, sz_size_t length, sz_ptr_t result);

/**
 *  @brief  Equivalent to `for (char & c : text) c = toascii(c)`.
 *
 *  @param text     String to be normalized.
 *  @param length   Number of bytes in the string.
 *  @param result   Output string, can point to the same address as ::text.
 */
SZ_PUBLIC void sz_toascii(sz_cptr_t text, sz_size_t length, sz_ptr_t result);
SZ_PUBLIC void sz_toascii_serial(sz_cptr_t text, sz_size_t length, sz_ptr_t result);
SZ_PUBLIC void sz_toascii_avx512(sz_cptr_t text, sz_size_t length, sz_ptr_t result);

/**
 *  @brief  Estimates the amount of temporary memory required to efficiently compute the edit distance.
 *
 *  @param a_length Number of bytes in the first string.
 *  @param b_length Number of bytes in the second string.
 *  @return         Number of bytes to allocate for temporary memory.
 */
SZ_PUBLIC sz_size_t sz_levenshtein_memory_needed(sz_size_t a_length, sz_size_t b_length);

/**
 *  @brief  Computes Levenshtein edit-distance between two strings.
 *          Similar to the Needleman–Wunsch algorithm. Often used in fuzzy string matching.
 *
 *  @param a        First string to compare.
 *  @param a_length Number of bytes in the first string.
 *  @param b        Second string to compare.
 *  @param b_length Number of bytes in the second string.
 *  @param buffer   Temporary memory buffer of size ::sz_levenshtein_memory_needed(a_length, b_length).
 *  @param bound    Upper bound on the distance, that allows us to exit early.
 *  @return         Unsigned edit distance.
 */
SZ_PUBLIC sz_size_t sz_levenshtein(sz_cptr_t a, sz_size_t a_length, sz_cptr_t b, sz_size_t b_length, //
                                   sz_ptr_t buffer, sz_size_t bound);
SZ_PUBLIC sz_size_t sz_levenshtein_serial(sz_cptr_t a, sz_size_t a_length, sz_cptr_t b, sz_size_t b_length, //
                                          sz_ptr_t buffer, sz_size_t bound);
SZ_PUBLIC sz_size_t sz_levenshtein_avx512(sz_cptr_t a, sz_size_t a_length, sz_cptr_t b, sz_size_t b_length, //
                                          sz_ptr_t buffer, sz_size_t bound);

/**
 *  @brief  Estimates the amount of temporary memory required to efficiently compute the weighted edit distance.
 *
 *  @param a_length Number of bytes in the first string.
 *  @param b_length Number of bytes in the second string.
 *  @return         Number of bytes to allocate for temporary memory.
 */
SZ_PUBLIC sz_size_t sz_alignment_score_memory_needed(sz_size_t a_length, sz_size_t b_length);

/**
 *  @brief  Computes Levenshtein edit-distance between two strings, parameterized for gap and substitution penalties.
 *          Similar to the Needleman–Wunsch algorithm. Often used in bioinformatics and cheminformatics.
 *
 *  This function is equivalent to the default Levenshtein distance implementation with the ::gap parameter set
 *  to one, and the ::subs matrix formed of all ones except for the main diagonal, which is zeros.
 *  Unlike the default Levenshtein implementaion, this can't be bounded, as the substitution costs can be both positive
 *  and negative, meaning that the distance isn't monotonically growing as we go through the strings.
 *
 *  @param a        First string to compare.
 *  @param a_length Number of bytes in the first string.
 *  @param b        Second string to compare.
 *  @param b_length Number of bytes in the second string.
 *  @param gap      Penalty cost for gaps - insertions and removals.
 *  @param subs     Substitution costs matrix with 256 x 256 values for all pais of characters.
 *  @param buffer   Temporary memory buffer of size ::sz_alignment_score_memory_needed(a_length, b_length).
 *  @return         Signed score ~ edit distance.
 */
SZ_PUBLIC sz_ssize_t sz_alignment_score(sz_cptr_t a, sz_size_t a_length, sz_cptr_t b, sz_size_t b_length, //
                                        sz_error_cost_t gap, sz_error_cost_t const *subs, sz_ptr_t buffer);
SZ_PUBLIC sz_ssize_t sz_alignment_score_serial(sz_cptr_t a, sz_size_t a_length, sz_cptr_t b, sz_size_t b_length, //
                                               sz_error_cost_t gap, sz_error_cost_t const *subs, sz_ptr_t buffer);
SZ_PUBLIC sz_ssize_t sz_alignment_score_avx512(sz_cptr_t a, sz_size_t a_length, sz_cptr_t b, sz_size_t b_length, //
                                               sz_error_cost_t gap, sz_error_cost_t const *subs, sz_ptr_t buffer);

/**
 *  @brief  Checks if two string are equal, and reports the first mismatch if they are not.
 *          Similar to `memcmp(a, b, length) == 0` in LibC and `a.starts_with(b)` in STL.
 *
 *  The implementation of this function is very similar to `sz_order`, but the usage patterns are different.
 *  This function is more often used in parsing, while `sz_order` is often used in sorting.
 *  It works best on platforms with cheap
 *
 *  @param a        First string to compare.
 *  @param b        Second string to compare.
 *  @param length   Number of bytes in both strings.
 *  @return         Null if strings match. Otherwise, the pointer to the first non-matching character in ::a.
 */
SZ_PUBLIC sz_cptr_t sz_mismatch_first(sz_cptr_t a, sz_cptr_t b, sz_size_t length);
SZ_PUBLIC sz_cptr_t sz_mismatch_first_serial(sz_cptr_t a, sz_cptr_t b, sz_size_t length);
SZ_PUBLIC sz_cptr_t sz_mismatch_first_avx512(sz_cptr_t a, sz_cptr_t b, sz_size_t length);

/**
 *  @brief  Checks if two string are equal, and reports the @b last mismatch if they are not.
 *          Similar to `memcmp(a, b, length) == 0` in LibC and `a.ends_with(b)` in STL.
 *
 *  @param a        First string to compare.
 *  @param b        Second string to compare.
 *  @param length   Number of bytes in both strings.
 *  @return         Null if strings match. Otherwise, the pointer to the last non-matching character in ::a.
 */
SZ_PUBLIC sz_cptr_t sz_mismatch_last(sz_cptr_t a, sz_cptr_t b, sz_size_t length);
SZ_PUBLIC sz_cptr_t sz_mismatch_last_serial(sz_cptr_t a, sz_cptr_t b, sz_size_t length);
SZ_PUBLIC sz_cptr_t sz_mismatch_last_avx512(sz_cptr_t a, sz_cptr_t b, sz_size_t length);

#pragma region String Sequences

struct sz_sequence_t;

typedef sz_cptr_t (*sz_sequence_member_start_t)(struct sz_sequence_t const *, sz_size_t);
typedef sz_size_t (*sz_sequence_member_length_t)(struct sz_sequence_t const *, sz_size_t);
typedef sz_bool_t (*sz_sequence_predicate_t)(struct sz_sequence_t const *, sz_size_t);
typedef sz_bool_t (*sz_sequence_comparator_t)(struct sz_sequence_t const *, sz_size_t, sz_size_t);
typedef sz_bool_t (*sz_string_is_less_t)(sz_cptr_t, sz_size_t, sz_cptr_t, sz_size_t);

typedef struct sz_sequence_t {
    sz_u64_t *order;
    sz_size_t count;
    sz_sequence_member_start_t get_start;
    sz_sequence_member_length_t get_length;
    void const *handle;
} sz_sequence_t;

/**
 *  @brief  Initiates the sequence structure from a tape layout, used by Apache Arrow.
 *          Expects ::offsets to contains `count + 1` entries, the last pointing at the end
 *          of the last string, indicating the total length of the ::tape.
 */
SZ_PUBLIC void sz_sequence_from_u32tape(sz_cptr_t *start, sz_u32_t const *offsets, sz_size_t count,
                                        sz_sequence_t *sequence);

/**
 *  @brief  Initiates the sequence structure from a tape layout, used by Apache Arrow.
 *          Expects ::offsets to contains `count + 1` entries, the last pointing at the end
 *          of the last string, indicating the total length of the ::tape.
 */
SZ_PUBLIC void sz_sequence_from_u64tape(sz_cptr_t *start, sz_u64_t const *offsets, sz_size_t count,
                                        sz_sequence_t *sequence);

/**
 *  @brief  Similar to `std::partition`, given a predicate splits the sequence into two parts.
 *          The algorithm is unstable, meaning that elements may change relative order, as long
 *          as they are in the right partition. This is the simpler algorithm for partitioning.
 */
SZ_PUBLIC sz_size_t sz_partition(sz_sequence_t *sequence, sz_sequence_predicate_t predicate);

/**
 *  @brief  Inplace `std::set_union` for two consecutive chunks forming the same continuous `sequence`.
 *
 *  @param partition The number of elements in the first sub-sequence in `sequence`.
 *  @param less Comparison function, to determine the lexicographic ordering.
 */
SZ_PUBLIC void sz_merge(sz_sequence_t *sequence, sz_size_t partition, sz_sequence_comparator_t less);

/**
 *  @brief  Sorting algorithm, combining Radix Sort for the first 32 bits of every word
 *          and a follow-up by a more conventional sorting procedure on equally prefixed parts.
 */
SZ_PUBLIC void sz_sort(sz_sequence_t *sequence);

/**
 *  @brief  Partial sorting algorithm, combining Radix Sort for the first 32 bits of every word
 *          and a follow-up by a more conventional sorting procedure on equally prefixed parts.
 */
SZ_PUBLIC void sz_sort_partial(sz_sequence_t *sequence, sz_size_t n);

/**
 *  @brief  Intro-Sort algorithm that supports custom comparators.
 */
SZ_PUBLIC void sz_sort_intro(sz_sequence_t *sequence, sz_sequence_comparator_t less);

#pragma endregion

#pragma region Compiler Extensions

/*
 *  Intrinsics aliases for MSVC, GCC, and Clang.
 */
#ifdef _MSC_VER
#define sz_popcount64 __popcnt64
#define sz_ctz64 _tzcnt_u64
#define sz_clz64 _lzcnt_u64
#else
#define sz_popcount64 __builtin_popcountll
#define sz_ctz64 __builtin_ctzll
#define sz_clz64 __builtin_clzll
#endif

/*
 *  Efficiently computing the minimum and maximum of two or three values can be tricky.
 *  The simple branching baseline would be:
 *
 *      x < y ? x : y                               // 1 conditional move
 *
 *  Branchless approach is well known for signed integers, but it doesn't apply to unsigned ones.
 *  https://stackoverflow.com/questions/514435/templatized-branchless-int-max-min-function
 *  https://graphics.stanford.edu/~seander/bithacks.html#IntegerMinOrMax
 *  Using only bitshifts for singed integers it would be:
 *
 *      y + ((x - y) & (x - y) >> 31)               // 4 unique operations
 *
 *  Alternatively, for any integers using multiplication:
 *
 *      (x > y) * y + (x <= y) * x                  // 5 operations
 *
 *  Alternatively, to avoid multiplication:
 *
 *      x & ~((x < y) - 1) + y & ((x < y) - 1)      // 6 unique operations
 */
#define sz_min_of_two(x, y) (x < y ? x : y)
#define sz_min_of_three(x, y, z) sz_min_of_two(x, sz_min_of_two(y, z))

/**
 *  @brief Branchless minimum function for two integers.
 */
inline static sz_i32_t sz_i32_min_of_two(sz_i32_t x, sz_i32_t y) { return y + ((x - y) & (x - y) >> 31); }

/**
 *  @brief Reverse the byte order of a 64-bit unsigned integer.
 *
 *  @note This function uses compiler-specific intrinsics to achieve the
 *        byte-reversal. It's designed to work with both MSVC and GCC/Clang.
 */
inline static sz_u64_t sz_u64_byte_reverse(sz_u64_t val) {
#ifdef _MSC_VER
    return _byteswap_uint64(val);
#else
    return __builtin_bswap64(val);
#endif
}

/**
 *  @brief  Compute the logarithm base 2 of an integer.
 *
 *  @note If n is 0, the function returns 0 to avoid undefined behavior.
 *  @note This function uses compiler-specific intrinsics or built-ins
 *        to achieve the computation. It's designed to work with GCC/Clang and MSVC.
 */
inline static sz_size_t sz_log2i(sz_size_t n) {
    if (n == 0) return 0;

#ifdef _WIN64
#ifdef _MSC_VER
    unsigned long index;
    if (_BitScanReverse64(&index, n)) return index;
    return 0; // This line might be redundant due to the initial check, but it's safer to include it.
#else
    return 63 - __builtin_clzll(n);
#endif
#elif defined(_WIN32)
#ifdef _MSC_VER
    unsigned long index;
    if (_BitScanReverse(&index, n)) return index;
    return 0; // Same note as above.
#else
    return 31 - __builtin_clz(n);
#endif
#else
// Handle non-Windows platforms. You can further differentiate between 32-bit and 64-bit if needed.
#if defined(__LP64__)
    return 63 - __builtin_clzll(n);
#else
    return 31 - __builtin_clz(n);
#endif
#endif
}

/**
 *  @brief Exports up to 4 bytes of the given string into a 32-bit scalar.
 */
inline static void sz_export_prefix_u32( //
    sz_cptr_t text, sz_size_t length, sz_u32_t *prefix_out, sz_u32_t *mask_out) {

    union {
        sz_u32_t u32;
        sz_u8_t u8s[4];
    } prefix, mask;

    switch (length) {
    case 1:
        mask.u8s[0] = 0xFF, mask.u8s[1] = mask.u8s[2] = mask.u8s[3] = 0;
        prefix.u8s[0] = text[0], prefix.u8s[1] = prefix.u8s[2] = prefix.u8s[3] = 0;
        break;
    case 2:
        mask.u8s[0] = mask.u8s[1] = 0xFF, mask.u8s[2] = mask.u8s[3] = 0;
        prefix.u8s[0] = text[0], prefix.u8s[1] = text[1], prefix.u8s[2] = prefix.u8s[3] = 0;
        break;
    case 3:
        mask.u8s[0] = mask.u8s[1] = mask.u8s[2] = 0xFF, mask.u8s[3] = 0;
        prefix.u8s[0] = text[0], prefix.u8s[1] = text[1], prefix.u8s[2] = text[2], prefix.u8s[3] = 0;
        break;
    default:
        mask.u32 = 0xFFFFFFFF;
        prefix.u8s[0] = text[0], prefix.u8s[1] = text[1], prefix.u8s[2] = text[2], prefix.u8s[3] = text[3];
        break;
    }
    *prefix_out = prefix.u32;
    *mask_out = mask.u32;
}

/**
 *  @brief  Internal data-structure, used to address "anomalies" (often prefixes),
 *          during substring search. Always a 32-bit unsigned integer, containing 4 chars.
 */
typedef union _sz_anomaly_t {
    sz_u32_t u32;
    sz_u8_t u8s[4];
} _sz_anomaly_t;

typedef struct sz_string_view_t {
    sz_cptr_t start;
    sz_size_t length;
} sz_string_view_t;

/**
 *  @brief  Helper structure to simpify work with 64-bit words.
 */
typedef union sz_u64_parts_t {
    sz_u64_t u64;
    sz_u32_t u32s[2];
    sz_u16_t u16s[4];
    sz_u8_t u8s[8];
} sz_u64_parts_t;

#pragma endregion

#ifdef __cplusplus
}
#endif

#endif // STRINGZILLA_H_