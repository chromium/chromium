---
name: general/prefer-lightweight-non-cryptographic-hash-for-identifiers
rubric: design
severity: should_fix
---

**Rule.** Do not implement or use complex cryptographic hash algorithms (such as
MD5 or SHA-1) and truncate their digest when generating non-cryptographic
hashes, type IDs, or identifier tokens. Use lightweight non-cryptographic hash
functions such as FNV-1a, MurmurHash, base::FastHash, or base::PersistentHash
instead.

**Why.** Cryptographic hash functions are complex, slow, and burdensome to
implement in compile-time constexpr contexts. Truncating cryptographic digests
to 32-bit tokens is misleading and over-engineered, whereas non-cryptographic
hashes like FNV-1a can be implemented in a few lines of constexpr code and
directly produce 32-bit values.

## Examples

Bad:

```cpp
// 250+ lines of constexpr MD5 rounds and padding, truncating a 128-bit digest to 32 bits:
constexpr uint32_t MD5Hash32Constexpr(std::string_view string) {
  return internal::MD5CE::Hash32(string);
}
```

Good:

```cpp
consteval uint32_t fnv1a_hash(const std::string_view s) {
  uint32_t hash = 2166136261u;
  for (auto ch : s) {
    hash ^= static_cast<uint32_t>(ch) & 0xFF;
    hash *= 1677619;
  }
  return hash;
}
```
