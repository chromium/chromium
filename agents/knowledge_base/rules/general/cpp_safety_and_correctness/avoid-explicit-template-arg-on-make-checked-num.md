---
name: general/avoid-explicit-template-arg-on-make-checked-num
rubric: correctness
severity: nit
---

**Rule.** Do not use `base::MakeCheckedNum<T>(val)` with an explicit template
argument to convert or construct a `CheckedNumeric<T>` from a value of a
different type. Use `base::CheckedNumeric<T>(val)` or
`base::MakeCheckedNum(val).Cast<T>()` instead.

**Why.** Factory functions like `MakeCheckedNum<T>(T value)` take the argument
by type `T`, which forces standard C++ implicit conversion prior to the call and
silently truncates or wraps out-of-range values. In contrast,
`CheckedNumeric<T>` provides a templated converting constructor
`template <typename Src> CheckedNumeric(Src value)` that detects out-of-range
values and sets the error state.

## Examples

Bad:

```cpp
int minus = -1;
// Explicit template argument implicitly coerces `minus` to uint32_t before the call,
// silently wrapping -1 to 0xFFFFFFFF and bypassing range validation.
base::CheckedNumeric<uint32_t> val = base::MakeCheckedNum<uint32_t>(minus);
```

Good:

```cpp
int minus = -1;
// CheckedNumeric constructor preserves the source type `int` and marks the numeric invalid.
base::CheckedNumeric<uint32_t> val = base::CheckedNumeric<uint32_t>(minus);
// Alternatively, deduce the source type and explicitly cast via CheckedNumeric::Cast.
auto checked_cast_val = base::MakeCheckedNum(minus).Cast<uint32_t>();
```
