---
name: general/avoid-cstr-in-repr-c-structs
rubric: safety
severity: should_fix
---

**Rule.** In `#[repr(C)]` Rust structs designed to mirror C or C++ struct
layouts containing raw string pointers (`const char*`), do not use fat-pointer
types like `&CStr` or `&str`, which break ABI compatibility by using two machine
words (data pointer and length). Use single-word thin pointers instead: prefer
`&'static c_char` over raw `*const c_char` when the string has static lifetime
and is non-null to retain compile-time lifetime and non-null safety guarantees,
reserving `*const c_char` for nullable or dynamic pointers.

**Why.** In Rust, `CStr` is a dynamically sized type (DST), so `&CStr` is a
two-word fat pointer (address and length) that doubles the field size and
misaligns subsequent struct fields against C++ `const char*` layouts. While
`*const c_char` matches the single-word C ABI layout, `&'static c_char` provides
the same thin-pointer ABI layout while enforcing compile-time non-null and
lifetime invariants without requiring unsafe pointer dereferences.

## Examples

Bad:

```cpp
#[repr(C)]
pub struct Feature {
    // Bad: &CStr is a fat pointer (ptr + len), breaking ABI compatibility with C++ const char*.
    name_fat: &'static std::ffi::CStr,
    // Bad: *const c_char is a thin pointer matching ABI, but loses Rust non-null and lifetime safety.
    name_raw: *const std::ffi::c_char,
    pub default_state: FeatureState,
    pub cached_value: AtomicU32,
}
```

Good:

```cpp
#[repr(C)]
pub struct Feature {
    // Good: &'static c_char is a thin pointer matching C++ const char* ABI while guaranteeing non-null and static lifetime.
    name: &'static std::ffi::c_char,
    pub default_state: FeatureState,
    pub cached_value: AtomicU32,
}
```
