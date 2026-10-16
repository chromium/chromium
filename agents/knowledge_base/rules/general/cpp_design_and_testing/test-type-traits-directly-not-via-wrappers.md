---
name: general/test-type-traits-directly-not-via-wrappers
rubric: design
severity: should_fix
---

**Rule.** When verifying type traits, promotion rules, or conversion mechanics,
assert against the underlying trait templates directly with static_assert rather
than testing them indirectly through wrapper expressions. Document illustrative
wrapper promotion behavior and usage examples in component documentation instead
of adding redundant wrapper unit tests.

**Why.** Testing underlying traits indirectly through wrapper expressions
creates redundant unit tests that obscure what is being verified when the traits
are already directly tested. Direct static_assert assertions pinpoint trait
regressions cleanly, while high-level wrapper behavior is best demonstrated in
documentation.

## Examples

Bad:

```cpp
TEST(SafeNumerics, ImplicitConversionAndPromotion) {
  uint16_t is16u = 65535;
  static_assert(std::is_same_v<decltype(CheckedNumeric(is16u) + 2),
                               CheckedNumeric<int32_t>>);
  static_assert(
      std::is_same_v<decltype(CheckedNumeric<uint16_t>(is16u) + is16u),
                     CheckedNumeric<uint16_t>>);
}
```

Good:

```cpp
// Test the underlying promotion traits directly:
static_assert(std::same_as<BigEnoughPromotion<int16_t, int8_t>, int16_t>);
static_assert(
    std::same_as<FastIntegerArithmeticPromotion<int16_t, int8_t>, int32_t>);
// Document higher-level wrapper usage and examples in component documentation.
```
