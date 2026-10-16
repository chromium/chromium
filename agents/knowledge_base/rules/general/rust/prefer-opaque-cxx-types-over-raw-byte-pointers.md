---
name: general/prefer-opaque-cxx-types-over-raw-byte-pointers
rubric: readability
severity: should_fix
---

**Rule.** In Rust-C++ CXX FFI bridges and shims, declare C++ types that cannot
be defined as shared CXX structs (such as types containing std::atomic fields or
complex layouts) as opaque types in an unsafe extern "C++" block using type T;
and include!(...). Pass these types across the boundary by reference (const T&
in C++, &T in Rust) rather than passing untyped raw byte pointers (const
uint8_t\* / \*const u8) or casting pointers.

**Why.** Passing raw byte pointers (const uint8_t\* or \*const u8) across FFI
boundaries loses compile-time type safety and discards alignment guarantees
(since byte pointers only require 1-byte alignment, casting them back to
structured C++ types risks unaligned memory access and undefined behavior).
Declaring opaque CXX types guarantees alignment through references, enforces
compile-time type checks, and eliminates unsafe reinterpret_cast in C++ shims.

## Examples

Bad:

```cpp
// C++ shim (base/feature_rust_shim.h)
bool IsFeatureEnabled(const uint8_t* feature);

// Rust CXX bridge (base/feature.rs)
#[cxx::bridge(namespace = "base")]
mod ffi {
    unsafe extern "C++" {
        include!("base/feature_rust_shim.h");
        unsafe fn IsFeatureEnabled(feature: *const u8) -> bool;
    }
}
```

Good:

```cpp
// C++ shim (base/feature_rust_shim.h)
bool IsFeatureEnabled(const Feature& feature);

// Rust CXX bridge (base/feature.rs)
#[cxx::bridge(namespace = "base")]
mod ffi {
    unsafe extern "C++" {
        include!("base/feature.h");
        type Feature;

        include!("base/feature_rust_shim.h");
        fn IsFeatureEnabled(feature: &Feature) -> bool;
    }
}
```
