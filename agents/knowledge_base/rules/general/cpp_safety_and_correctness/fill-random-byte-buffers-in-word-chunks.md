---
name: general/fill-random-byte-buffers-in-word-chunks
rubric: safety
severity: should_fix
---

**Rule.** When populating byte buffers or spans with pseudo-random numbers in
fuzzers, tests, or benchmarks, generate data in 32-bit or word-sized chunks
(e.g., via reinterpret_span\<uint32_t>) and fill any trailing bytes from a
single generated word rather than querying the PRNG engine once per individual
byte.

**Why.** PRNG engines (such as std::minstd_rand0 or std::mt19937) compute 32-bit
or 64-bit words per invocation. Calling the generator per single byte discards
the remaining entropy bits and incurs unnecessary function call overhead,
degrading fuzzer and benchmark throughput.

## Examples

Bad:

```cpp
// Bad: Querying a 32-bit PRNG per individual byte discards entropy and adds overhead.
for (uint8_t& val : dst) {
  val = static_cast<uint8_t>(rng());
}
```

Good:

```cpp
// Good: Fill in 32-bit chunks, then slice trailing bytes from a single generated word.
const size_t aligned_bytes = (dst.size() / sizeof(uint32_t)) * sizeof(uint32_t);
auto [aligned_dst, trailing_dst] = dst.split_at(aligned_bytes);
for (uint32_t& val : base::subtle::reinterpret_span<uint32_t>(aligned_dst)) {
  val = rng();
}
if (!trailing_dst.empty()) {
  uint32_t trailing_rand = rng();
  trailing_dst.copy_from(
      base::as_bytes(base::span_from_ref(trailing_rand)).first(trailing_dst.size()));
}
```
