---
name: general/use-c-char-for-ffi-in-rust
rubric: correctness
severity: should_fix
---

**Rule.** In Rust code interfacing with C/C++ APIs, representing C-compatible
struct fields, or casting character pointers, use `std::ffi::c_char` (or
`core::ffi::c_char`) rather than hardcoding `i8` or `u8` for C `char` values,
pointers (`*const c_char`, `*mut c_char`), or slices. When casting raw byte
pointers (such as from `.as_ptr()`) or declaring `extern "C"` functions, cast to
or accept `c_char`.

**Why.** In C, `char` signedness is target-architecture dependent: signed (`i8`)
on x86/x64, but unsigned (`u8`) on ARM/AArch64. Rust's `std::ffi::c_char` is a
target-dependent type alias that maps to the correct integer type for the target
ABI. Hardcoding `i8` or `u8` causes cross-platform compilation errors or ABI
type mismatches on ARM targets.

## Examples

Bad:

```cpp
// Hardcoding *const i8 or *mut i8 fails on ARM/AArch64 where C char is u8.
pub struct Feature {
    name: *const i8,
}

extern "C" {
    fn print_message(msg: *const i8);
}
```

Good:

```cpp
use std::ffi::c_char;

// c_char resolves to i8 or u8 depending on target architecture.
pub struct Feature {
    name: *const c_char,
}

extern "C" {
    fn print_message(msg: *const c_char);
}
```
