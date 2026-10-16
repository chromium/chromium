---
name: general/reuse-member-helpers-in-constexpr-methods
rubric: design
severity: should_fix
---

**Rule.** When making constructors or member functions constexpr, reuse existing
member helper methods (such as Put(), Insert(), or element mutators) rather than
bypassing class encapsulation with raw manipulations of underlying data members.
Mark the mutating helper methods constexpr as well so they can be shared across
runtime and compile-time contexts.

**Why.** Directly manipulating internal data members bypasses class
encapsulation and duplicates logic already implemented in helper methods. In
modern C++, mutating member methods can be constexpr, so helpers should be
marked constexpr and reused rather than bypassed.

## Examples

Bad:

```cpp
constexpr EnumSet(std::initializer_list<E> values) {
  for (E value : values) {
    enums_.set(ToIndex(value));
  }
}
```

Good:

```cpp
constexpr EnumSet(std::initializer_list<E> values) {
  for (E value : values) {
    Put(value);
  }
}

constexpr void Put(E value) { enums_.set(ToIndex(value)); }
```
