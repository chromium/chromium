---
name: general/reuse-canonical-cpp-entry-points-for-ffi
rubric: design
severity: should_fix
---

**Rule.** When exposing C++ APIs across FFI boundaries (such as Rust `cxx`
bridges), bind to existing canonical C++ entry points rather than adding
redundant convenience methods or duplicate entry points to C++ classes. If an
existing C++ function cannot be called directly across the bridge (for example,
due to untranslatable types), create wrapper functions in a dedicated C++ shim
file (`*_shim.h`/`.cc`) instead of expanding core C++ class interfaces.

**Why.** Adding alternative entry points to C++ types to accommodate FFI
declarations unnecessarily expands the public C++ API surface and violates the
principle of having a single canonical way to perform an operation. Reusing
existing canonical entry points or isolating translation logic in dedicated shim
files keeps C++ class interfaces clean and prevents API fragmentation.

## Examples

Bad:

```cpp
// base/feature.h
struct BASE_EXPORT LOGICALLY_CONST Feature {
  // Bad: Adding a redundant entry point to C++ class solely to avoid
  // declaring FeatureList in the FFI bridge.
  static bool IsEnabled(const Feature& feature);
};
```

Good:

```cpp
// base/feature_list.h
class BASE_EXPORT FeatureList {
  static bool IsEnabled(const Feature& feature);
};

// In Rust cxx bridge or C++ shim:
// Reuse the existing canonical entry point FeatureList::IsEnabled.
#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("base/feature_list.h");
        type Feature;
        #[rust_name = "is_feature_enabled"]
        fn IsEnabled(feature: &Feature) -> bool;
    }
}
```
