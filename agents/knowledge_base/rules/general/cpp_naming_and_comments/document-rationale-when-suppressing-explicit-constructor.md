---
name: general/document-rationale-when-suppressing-explicit-constructor
rubric: readability
severity: should_fix
---

**Rule.** When intentionally omitting explicit on a single-argument or
converting constructor, suppress the Clang-Tidy diagnostic using //
NOLINTNEXTLINE(google-explicit-constructor) and add an explanatory comment
documenting why implicit conversion is intended, safe, and desirable.

**Why.** Single-argument constructors in Chromium must generally be explicit to
prevent accidental implicit type conversions. When implicit conversion is
deliberate (such as widening PassKey specializations or converting wrappers),
documenting the rationale and annotating with NOLINTNEXTLINE clarifies design
intent for human reviewers while silencing automated linter warnings.

## Examples

Bad:

```cpp
// Implicit constructor without rationale comment or suppression.
template <typename T>
  requires pass_key_internal::OneOf<T, Args...>
PassKey(PassKey<T>) {}
```

Good:

```cpp
// Allows constructing a PassKey<A, B> from PassKey<A> or PassKey<B>.
// Implicit to allow feeding PassKey<A> into a function that expects
// PassKey<A, B>.
template <typename T>
  requires pass_key_internal::OneOf<T, Args...>
// NOLINTNEXTLINE(google-explicit-constructor)
PassKey(PassKey<T>) {}
```
