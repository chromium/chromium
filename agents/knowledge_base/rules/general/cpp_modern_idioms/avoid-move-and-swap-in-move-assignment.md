---
name: general/avoid-move-and-swap-in-move-assignment
rubric: readability
severity: should_fix
---

**Rule.** When implementing move assignment operators (`operator=(Type&&)`) for
RAII or resource-managing classes, avoid using temporary move-and-swap idioms
(such as `Type(std::move(rhs)).swap(*this)`). Instead, explicitly release or
reset the existing resource—sharing a `Reset()` helper with the destructor—and
move or exchange member state directly from the source object.

**Why.** Move-and-swap relies on constructing a temporary object and destroying
it during assignment, which introduces unnecessary temporary overhead, obscures
the exact timing of resource cleanup, and often requires adding a superfluous
`swap()` helper. Directly resetting the current resource and transferring
members via `std::exchange` or `std::move` makes ownership transfer explicit,
predictable, and clean.

## Examples

Bad:

```cpp
// Bad: Using move-and-swap with a temporary in move assignment.
AutoReset& operator=(AutoReset&& rhs) {
  AutoReset(std::move(rhs)).swap(*this);
  return *this;
}
```

Good:

```cpp
// Good: Explicitly reset existing resource and transfer members directly.
AutoReset& operator=(AutoReset&& rhs) {
  Reset();
  scoped_variable_ = std::exchange(rhs.scoped_variable_, nullptr);
  original_value_ = std::move(rhs.original_value_);
  return *this;
}
```
