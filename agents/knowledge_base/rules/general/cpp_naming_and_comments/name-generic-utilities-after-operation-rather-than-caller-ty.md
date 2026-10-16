---
name: general/name-generic-utilities-after-operation-rather-than-caller-ty
rubric: readability
severity: nit
---

**Rule.** Do not name general utility functions, types, or header files after a
specific caller or consumer type (such as `Span` or container classes) when the
implementation operates on generic primitive parameters (such as `uintptr_t`,
`void*`, and `size_t`). Name the utilities and headers after the general
concept, operation, or invariant being checked (such as `extent` or
`bounds_check`).

**Why.** Naming utility functions or headers after a specific caller type
misleadingly implies a dependency or tight coupling that does not exist,
confusing readers and obscuring or discouraging reuse across different caller
contexts.

## Examples

Bad:

```cpp
// In span_check.h:
// Bad: Named after a specific caller ('Span') even though it operates on raw address and size.
bool PartitionAllocSaysSpanIsOutOfBounds(uintptr_t address, size_t size);
```

Good:

```cpp
// In bounds_checks.h:
// Good: Named after the general concept and operation ('Extent', 'OutOfBounds').
bool IsExtentOutOfBounds(const void* address, size_t size_bytes, size_t type_size);
```
