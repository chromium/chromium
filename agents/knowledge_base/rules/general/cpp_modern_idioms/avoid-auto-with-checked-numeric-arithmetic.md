---
name: general/avoid-auto-with-checked-numeric-arithmetic
rubric: readability
severity: nit
---

**Rule.** Do not use `auto` to deduce the type of a `base::CheckedNumeric` from
infix arithmetic expressions. Instead, explicitly spell out the target
`CheckedNumeric<T>` type, use compound assignment operators (such as `+=`, `-=`,
`*=`, `/=`), or invoke `.Cast<T>()` to prevent unintended type widening and
preserve overflow detection.

**Why.** Infix arithmetic operations on `CheckedNumeric` promote to the smallest
`CheckedNumeric` type capable of representing both operands, without defaulting
to `int`. Using `auto` can silently widen the underlying representation, masking
intended arithmetic overflow detection or altering signedness.

## Examples

Bad:

```cpp
// auto deduces CheckedNumeric<int>, widening from uint16_t and masking overflow.
auto result = base::CheckedNumeric<uint16_t>(65535) + 1;
```

Good:

```cpp
// Compound assignment preserves the underlying CheckedNumeric<uint16_t> type.
base::CheckedNumeric<uint16_t> val = 65535;
val += 1;  // Correctly marks the CheckedNumeric invalid due to overflow.

// Explicit typing or Cast<T>() ensures the desired width and overflow checks.
base::CheckedNumeric<uint16_t> result =
    (base::CheckedNumeric<uint16_t>(65535) + 1).Cast<uint16_t>();
```
