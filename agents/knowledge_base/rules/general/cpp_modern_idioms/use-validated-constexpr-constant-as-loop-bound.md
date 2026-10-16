---
name: general/use-validated-constexpr-constant-as-loop-bound
rubric: readability
severity: nit
---

**Rule.** When iterating over a container, string, or buffer whose length has
already been verified against a compile-time constant (e.g. via an early-return
size check, CHECK_EQ, or DCHECK_EQ), use the constexpr constant as the loop
limit instead of querying runtime methods like .length() or .size().

**Why.** Using the compile-time constant explicitly conveys the fixed-length
invariant to readers and allows the compiler to statically optimize loop bounds,
unrolling, and vectorization.

## Examples

Bad:

```cpp
constexpr size_t kExpectedLength = 36;
if (input.length() != kExpectedLength) {
  return false;
}
for (size_t i = 0; i < input.length(); ++i) {
  output[i] = ProcessByte(input[i]);
}
```

Good:

```cpp
constexpr size_t kExpectedLength = 36;
if (input.length() != kExpectedLength) {
  return false;
}
for (size_t i = 0; i < kExpectedLength; ++i) {
  output[i] = ProcessByte(input[i]);
}
```
