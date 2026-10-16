---
name: general/prefer-requires-clauses-over-static-assert
rubric: readability
severity: should_fix
---

**Rule.** Prefer C++20 requires clauses or concepts over function-body
static_assert statements to enforce template type traits, preconditions, and
interface contracts on function templates. Parenthesize constraint expressions
in the requires clause.

**Why.** Constraints declare interface contracts directly on template signatures
rather than deferring failures to function instantiation. This allows compilers
to report concise diagnostic explanations, integrates cleanly with overload
resolution, and participates in concept-based template metaprogramming.

## Examples

Bad:

```cpp
template <typename ElementType, typename ByteType, size_t Count>
constexpr auto reinterpret_span(span<ByteType, Count> s) {
  static_assert(std::is_trivially_copyable_v<ElementType>,
                "Target type must be trivially copyable.");
  static_assert(!std::is_const_v<ByteType> || std::is_const_v<ElementType>,
                "Cannot cast from const span to mutable span.");
  return span<ElementType, detail::ReinterpretSpanSize<ElementType, ByteType, Count>()>(...);
}
```

Good:

```cpp
template <typename ElementType, typename ByteType, size_t Count>
  requires(std::is_trivially_copyable_v<ElementType>) &&
          (!std::is_const_v<ByteType> || std::is_const_v<ElementType>)
constexpr auto reinterpret_span(span<ByteType, Count> s) {
  return span<ElementType, detail::ReinterpretSpanSize<ElementType, ByteType, Count>()>(...);
}
```
