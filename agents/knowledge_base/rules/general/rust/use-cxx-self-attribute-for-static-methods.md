---
name: general/use-cxx-self-attribute-for-static-methods
rubric: readability
severity: should_fix
---

**Rule.** In `cxx` FFI bridges, bind C++ static member functions directly to
their enclosing C++ type using the `#[Self = "TypeName"]` attribute (and
`#[rust_name = "..."]` if adapting naming conventions). Do not author custom C++
shim wrappers, wrapper headers, or free forwarding functions solely to expose
static C++ methods to Rust.

**Why.** Creating intermediate C++ shim files and forwarding functions adds
unnecessary indirection, boilerplate, and maintenance overhead. The `cxx` bridge
macro natively supports exposing C++ static member functions as Rust associated
functions via `#[Self = "TypeName"]`, generating idiomatic
`TypeName::method(...)` invocations directly without bespoke C++ shims.

## Examples

Bad:

```cpp
// In C++ shim (base/feature_rust_shim.h / feature_shim.cc):
namespace base {
bool FeatureList_IsEnabled(const Feature& feature);
}

// In Rust (base/feature.rs):
#[cxx::bridge]
mod ffi {
    unsafe extern "C++" {
        include!("base/feature.h");
        include!("base/feature_rust_shim.h");
        type Feature;

        fn FeatureList_IsEnabled(feature: &Feature) -> bool;
    }
}
```

Good:

```cpp
// In Rust (base/feature.rs):
#[cxx::bridge(namespace = "base")]
mod ffi {
    unsafe extern "C++" {
        include!("base/feature.h");
        include!("base/feature_list.h");

        type Feature;
        type FeatureList;

        #[Self = "FeatureList"]
        #[rust_name = "is_enabled"]
        fn IsEnabled(feature: &Feature) -> bool;
    }
}
```
