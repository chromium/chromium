---
name: general/manage-raii-guard-lifetime-via-block-scoping
rubric: readability
severity: optional
---

**Rule.** Do not wrap RAII scope guards (such as `base::AutoReset`,
`base::ScopedClosureRunner`, or custom scoped types) in `std::optional` to
manually manage their lifetime or invoke `.reset()` for early destruction. Use
explicit lexical block scoping (`{ ... }`) to delimit the lifetime of scope
guards, especially in unit tests where code structure serves as implicit
documentation for idiomatic API usage.

**Why.** RAII scope utilities are designed to bind resource acquisition and
cleanup to lexical C++ scopes in strict LIFO order. Wrapping scope guards in
`std::optional` merely to control when destruction happens creates unidiomatic
clutter, undermines the RAII idiom, and misleads readers about recommended
usage.

## Examples

Bad:

```cpp
std::optional<base::AutoReset<int>> resetter{std::in_place, &value, 20};
EXPECT_EQ(20, value);
resetter.reset();
EXPECT_EQ(10, value);
```

Good:

```cpp
{
  base::AutoReset<int> resetter{&value, 20};
  EXPECT_EQ(20, value);
}
EXPECT_EQ(10, value);
```
