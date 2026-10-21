---
name: base/place-specialized-data-structures-in-base-subtle
rubric: readability
severity: should_fix
---

**Rule.** In base, place low-level, hazardous, or easily misused utilities (such
as reinterpret_span) and specialized data structures intended strictly for
generated code, static tables, or relocation elimination (such as IndexPointer
or StringSlice) inside namespace base::subtle rather than directly in base.
Accompany declarations with doc comments explaining their specialized purpose,
constraints, and why general-purpose usage is discouraged.

**Why.** Doc comments warning callers to use sparingly or prefer higher-level
abstractions are easily overlooked. Enclosing hazardous utilities and
specialized data structures in base::subtle forces call sites to explicitly
qualify calls, signalling caution, preventing accidental casual usage across the
codebase, and keeping the top-level base API surface clean.

## Examples

Bad:

```cpp
namespace base {

// Specialized pointer offset for generated tables.
template <typename T, const T base[]>
class IndexPointer {
  // ...
};

// Please use sparingly. Prefer structured deserialization (e.g. SpanReader).
template <typename ElementType, typename ByteType, size_t Count>
constexpr auto reinterpret_span(span<ByteType, Count> s);

}  // namespace base
```

Good:

```cpp
namespace base::subtle {

// An IndexPointer stores a pointer as an index from a base object.
// This is intended only for use in generated static tables.
template <typename T, const T base[]>
class IndexPointer {
  // ...
};

// Please use sparingly. Prefer structured deserialization (e.g. SpanReader).
template <typename ElementType, typename ByteType, size_t Count>
constexpr auto reinterpret_span(span<ByteType, Count> s);

}  // namespace base::subtle
```
