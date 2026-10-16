---
name: general/pass-rust-str-and-string-view-across-ffi-boundaries
rubric: readability
severity: should_fix
---

**Rule.** When bridging string parameters across Rust/C++ FFI boundaries using
`cxx` (`#[cxx::bridge]`), declare Rust string parameters as `&str` and accept
them on the C++ side as `rust::Str` (converting to `std::string_view` or
`std::string` in C++ implementations), rather than passing raw character or byte
pointers (`*const c_char`, `*const i8`, `*const u8`, `const char*`,
`const int8_t*`) or requiring null-terminated strings (`CString`). If the
underlying C++ API takes `std::string_view` or `const std::string&`, convert
`rust::Str` to `std::string_view` in the C++ shim.

**Why.** `cxx` natively bridges Rust `&str` string slices to `rust::Str` on the
C++ side with zero-copy safety and direct conversion to `std::string_view`. Rust
`&str` is a slice (pointer and length) that is not guaranteed to be
null-terminated; requiring null-terminated C strings or raw byte/char pointers
forces Rust callers to allocate temporary heap memory via `CString`, use
`unsafe` blocks, and perform manual length calculations or pointer casts
(`reinterpret_cast`). Bridging `&str` to `rust::Str` and `std::string_view`
enables safe, zero-allocation, idiomatic string passing across language
boundaries.

## Examples

Bad:

```cpp
// Rust CXX bridge and caller using raw pointers or forcing CString allocation
#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        fn InitFromCommandLine(
            self: Pin<&mut Shim>,
            enable: *const c_char,
            disable: *const i8,
        );
    }
}
let enable = std::ffi::CString::new(enable_features).unwrap();
let disable = std::ffi::CString::new(disable_features).unwrap();
unsafe { shim.InitFromCommandLine(enable.as_ptr(), disable.as_ptr().cast()); }

// C++ shim taking raw pointers and casting
void Shim::InitFromCommandLine(const char* enable, const int8_t* disable) {
  scoped_feature_list_.InitFromCommandLine(
      enable, reinterpret_cast<const char*>(disable));
}
```

Good:

```cpp
// Rust CXX bridge passing safe string slices directly
#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        fn InitFromCommandLine(
            self: Pin<&mut Shim>,
            enable: &str,
            disable: &str,
        );
    }
}
shim.InitFromCommandLine(enable_features, disable_features);

// C++ shim accepting rust::Str and converting to std::string_view
void Shim::InitFromCommandLine(rust::Str enable, rust::Str disable) {
  scoped_feature_list_.InitFromCommandLine(
      std::string_view(enable), std::string_view(disable));
}
```
