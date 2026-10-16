---
name: general/validate-crypto-and-hash-with-standard-test-vectors
rubric: testing
severity: should_fix
---

**Rule.** When implementing, porting, or wrapping standard cryptographic or
hashing algorithms (such as SHA, MD5, AES, HMAC, or KDFs), include unit tests
that validate the implementation against authoritative published test vectors
from official specifications (such as RFCs, NIST SP, or FIPS publications).
Ensure tests cover standard test vectors for short messages, multi-block inputs,
boundary cases, and empty strings.

**Why.** Standard test vectors from authoritative specifications (RFC,
NIST/FIPS) verify mathematical correctness, block padding, alignment, and
endianness handling. Testing against ad-hoc inputs without published test
vectors risks missing subtle algorithmic, padding, or multi-block edge-case
defects.

## Examples

Bad:

```cpp
// Implementing or wrapping a standard hash algorithm without test coverage against published test vectors
constexpr uint32_t MD5Hash32Constexpr(std::string_view string);
```

Good:

```cpp
// Validating implementation against published RFC / NIST test vectors in unit tests
TEST(MD5Test, ValidateStandardVectors) {
  // Test vectors from RFC 1321 Section A.5
  EXPECT_EQ(MD5Hash(""), "d41d8cd98f00b204e9800998ecf8427e");
  EXPECT_EQ(MD5Hash("abc"), "900150983cd24fb0d6963f7d28e17f72");
  EXPECT_EQ(MD5Hash("message digest"), "f96b697d7cb7938d525a2f31aaf161d0");
}
```
