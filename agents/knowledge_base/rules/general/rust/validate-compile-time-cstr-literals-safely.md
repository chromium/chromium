---
name: general/validate-compile-time-cstr-literals-safely
rubric: correctness
severity: should_fix
---

**Rule.** When constructing &'static CStr values from string or byte literals in
Rust macros or const contexts, validate NUL-termination at compile time using
std::ffi::CStr::from_bytes_with_nul (panicking or asserting on error in a match
or const block) instead of unsafe CStr::from_bytes_with_nul_unchecked. Avoid
unnecessary unsafe blocks when compile-time validation can prove the string
contains no interior NUL bytes.

**Why.** CStr::from_bytes_with_nul_unchecked is unsafe and skips validation of
interior NUL bytes or proper termination, risking string truncation or buffer
overreads across FFI boundaries. In const contexts, CStr::from_bytes_with_nul
executes at compile time, eliminating unnecessary unsafe blocks while catching
malformed strings at build time.

## Examples

Bad:

```cpp
macro_rules! BASE_FEATURE {
    ($id:ident, $name:expr, $default:expr) => {
        static $id: $crate::Feature = $crate::Feature::new(
            unsafe {
                // Skips interior NUL byte validation and introduces an unnecessary unsafe block.
                std::ffi::CStr::from_bytes_with_nul_unchecked(concat!($name, "\0").as_bytes())
            },
            $default,
            $crate::internal::FeatureMacroHandshake::Secret,
        );
    };
}
```

Good:

```cpp
macro_rules! BASE_FEATURE {
    ($id:ident, $name:expr, $default:expr) => {
        static $id: $crate::Feature = $crate::Feature::new(
            {
                // Validates at compile time that the string is properly NUL-terminated with no interior NUL bytes.
                const C: &std::ffi::CStr =
                    match std::ffi::CStr::from_bytes_with_nul(concat!($name, "\0").as_bytes()) {
                        Ok(c) => c,
                        Err(_) => panic!("Feature name contains interior NUL"),
                    };
                C
            },
            $default,
            $crate::internal::FeatureMacroHandshake::Secret,
        );
    };
}
```
