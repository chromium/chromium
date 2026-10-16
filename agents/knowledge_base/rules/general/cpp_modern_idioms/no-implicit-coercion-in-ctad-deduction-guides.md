---
name: general/no-implicit-coercion-in-ctad-deduction-guides
rubric: readability
severity: should_fix
---

**Rule.** Do not define Class Template Argument Deduction (CTAD) deduction
guides that perform implicit type coercion (such as unwrapping enums to
underlying types). Deduction guides should deduce template argument types
directly, keeping conversions explicit at call sites.

**Why.** Implicit type coercion in deduction guides silently masks unintended
conversions and creates behavioral inconsistency across sibling template types.
Keeping deduction guides direct ensures conversions remain explicit and visible
at call sites.

## Examples

Bad:

```cpp
// Bad: deduction guide implicitly unwraps enums to their underlying types.
template <typename T>
ClampedNumeric(T) -> ClampedNumeric<UnderlyingType<T>>;
```

Good:

```cpp
// Good: deduction guide deduces template argument types directly without implicit coercion.
template <typename T>
ClampedNumeric(T) -> ClampedNumeric<T>;
```
