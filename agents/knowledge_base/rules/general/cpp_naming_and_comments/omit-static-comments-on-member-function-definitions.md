---
name: general/omit-static-comments-on-member-function-definitions
rubric: readability
severity: optional
---

**Rule.** Do not add '// static' comments above out-of-line definitions of
static member functions in .cc, .cpp, or .mm files. Rely on the static specifier
at the class declaration in the header rather than annotating definition lines
with redundant comments.

**Why.** The static keyword is already declared on the member function in the
header. Adding '// static' comments above definitions in implementation files is
an obsolete legacy convention that adds visual clutter and boilerplate without
conveying new invariants.

## Examples

Bad:

```cpp
// static
Pickle Pickle::WithData(span<const uint8_t> data) {
  Pickle pickle(kUnownedData, data);
  return pickle;
}
```

Good:

```cpp
Pickle Pickle::WithData(span<const uint8_t> data) {
  Pickle pickle(kUnownedData, data);
  return pickle;
}
```
