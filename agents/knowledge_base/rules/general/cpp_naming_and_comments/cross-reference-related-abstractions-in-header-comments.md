---
name: general/cross-reference-related-abstractions-in-header-comments
rubric: readability
severity: nit
---

**Rule.** When introducing or modifying a class, type, or utility that serves a
similar purpose or offers an alternative design to an existing abstraction,
cross-reference both abstractions in their top-level header comments. Explicitly
document the key trade-offs between them (such as storage overhead, access
latency, encoding representation, or safety constraints) so callers can choose
the appropriate abstraction.

**Why.** In a large codebase, multiple abstractions often exist to solve related
problems with different trade-offs. Cross-referencing alternative abstractions
in top-level header comments ensures developers discover existing solutions,
understand why alternatives exist, and pick the best fit for their use case.

## Examples

Bad:

```cpp
// In base/memory/index_pointer.h:
// An IndexPointer stores a pointer in the form of an index from a base object.
// This should only be used for pointers which are:
//  - constant
//  - stored in statically-allocated objects
//  - accessed rarely
```

Good:

```cpp
// In base/memory/index_pointer.h:
// An IndexPointer stores a pointer in the form of an index from a base object.
//
// See StringSlice for a similar type specifically for strings represented
// as slices (which needs a few more bytes than a C-style string but could
// be faster to access in some contexts).

// In base/strings/string_slice.h:
// See also IndexPointer, which can be used as IndexPointer<char, ...> for
// C-style strings without an explicitly stored length.
```
