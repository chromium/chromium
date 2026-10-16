---
name: general/iterate-scalar-types-by-value-in-range-for
rubric: readability
severity: should_fix
---

**Rule.** In range-based for loops over containers of small scalar, primitive,
or trivially copyable types (such as `char`, `int`, `uint8_t`, enum types, or
character typedefs like `CharType`), iterate by value and specify the explicit
type instead of using `const auto&`, `const Type&`, or plain `auto`. Reserve
`const auto&` or reference bindings for non-trivial or heavyweight objects.

**Why.** Small scalar and primitive types are cheap to copy and fit in CPU
registers. Binding them by reference introduces unnecessary pointer indirection
and potential aliasing restrictions, while spelling out the concrete type makes
the loop variable immediately clear and readable.

## Examples

Bad:

```cpp
for (const auto& separator : separators) {
  if (separator != normalized_separator) {
    std::ranges::replace(copy, separator, normalized_separator);
  }
}
```

Good:

```cpp
for (CharType separator : separators) {
  if (separator != normalized_separator) {
    std::ranges::replace(copy, separator, normalized_separator);
  }
}
```
