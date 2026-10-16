---
name: general/pass-prng-engines-by-reference
rubric: correctness
severity: should_fix
---

**Rule.** Do not pass pseudo-random number generator (PRNG) engines (such as
`std::minstd_rand0`, `std::mt19937`, or custom stateful RNGs) by value to helper
functions that draw random numbers; pass them by non-const reference
(`Engine&`). Passing by value creates a local copy, leaving the caller's
generator state unadvanced.

**Why.** PRNG engines maintain mutable internal state that advances with each
generated number. Passing an engine by value duplicates generator state, causing
subsequent helper invocations with the caller's generator to produce identical
pseudo-random sequences and reducing entropy in tests and fuzzers.

## Examples

Bad:

```cpp
// Passing PRNG engine by value makes a copy; caller state remains unchanged.
void FillBufferWithRandomData(base::span<uint8_t> dst,
                              std::minstd_rand0 rng) {
  for (uint8_t& byte : dst) {
    byte = static_cast<uint8_t>(rng());
  }
}

// Sequential calls receive identical pseudo-random streams:
FillBufferWithRandomData(src_y, rng);
FillBufferWithRandomData(src_u, rng);  // Duplicate data!
```

Good:

```cpp
// Passing PRNG engine by non-const reference advances caller state across calls.
void FillBufferWithRandomData(base::span<uint8_t> dst,
                              std::minstd_rand0& rng) {
  for (uint8_t& byte : dst) {
    byte = static_cast<uint8_t>(rng());
  }
}

// Sequential calls properly advance the PRNG sequence:
FillBufferWithRandomData(src_y, rng);
FillBufferWithRandomData(src_u, rng);
```
