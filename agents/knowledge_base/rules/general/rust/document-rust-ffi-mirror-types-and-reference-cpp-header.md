---
name: general/document-rust-ffi-mirror-types-and-reference-cpp-header
rubric: readability
severity: should_fix
---

**Rule.** When defining a Rust type that mirrors a C++ type for FFI
compatibility (such as `#[repr(C)]` structs or enums), document in doc comments
that it is the Rust equivalent of the C++ type, note that it must maintain an
identical memory layout for ABI compatibility, and direct readers to the
canonical C++ header for detailed behavioral documentation.

**Why.** Rust types mirroring C++ types must strictly maintain ABI and memory
layout compatibility across language boundaries. Explicit doc comments clarify
the layout constraint to maintainers and point readers to the authoritative C++
header as the single source of truth, avoiding duplicated and drift-prone
documentation.

## Examples

Bad:

```cpp
#[repr(C)]
pub struct Feature {
    name: *const c_char,
    pub default_state: FeatureState,
    pub cached_value: AtomicU32,
}
```

Good:

```cpp
/// Rust equivalent of the C++ `base::Feature` type.
///
/// This struct must maintain an identical memory layout to the C++ version
/// for FFI compatibility. Detailed documentation for feature flags can be
/// found in `base/feature.h`.
#[repr(C)]
pub struct Feature {
    name: *const c_char,
    pub default_state: FeatureState,
    pub cached_value: AtomicU32,
}
```
