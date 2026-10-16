---
name: general/document-api-adaptation-not-generic-ffi-lifetimes-on-shims
rubric: readability
severity: nit
---

**Rule.** When documenting C++/Rust FFI wrapper or shim classes, describe the
specific API and type translation the wrapper provides (such as converting
`rust::Str` or slices to C++ types) rather than attributing the wrapper's
existence to generic FFI lifetime management or heap allocation (such as
`cxx::UniquePtr` RAII destruction). Explain the actual bridging rationale or
omit generic lifetime commentary when standard `cxx::UniquePtr` ownership
applies.

**Why.** Opaque C++ types can already be heap-allocated and held via
`cxx::UniquePtr` in Rust to invoke destructors on drop without requiring wrapper
types. Explaining wrapper classes as necessary for lifetime management or RAII
misleads readers into believing underlying C++ classes cannot be managed
directly across FFI, obscuring the wrapper's true purpose of bridging
untranslatable parameter types.

## Examples

Bad:

```cpp
// ScopedFeatureListRs provides a stable C++/Rust FFI boundary for
// base::test::ScopedFeatureList.
//
// base::test::ScopedFeatureList relies on RAII to manage global feature state.
// While typically used on the stack in C++, Rust manages this object on the
// heap via cxx::UniquePtr.
//
// This ensures that the C++ destructor is correctly invoked when the Rust
// object is dropped, restoring the global FeatureList to its previous state.
class ScopedFeatureListRs { ... };
```

Good:

```cpp
// ScopedFeatureListRs provides a C++/Rust FFI interface for
// base::test::ScopedFeatureList, converting Rust string slices to C++
// Feature references.
class ScopedFeatureListRs { ... };
```
