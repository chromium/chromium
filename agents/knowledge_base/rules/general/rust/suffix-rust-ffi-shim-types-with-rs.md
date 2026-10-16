---
name: general/suffix-rust-ffi-shim-types-with-rs
rubric: readability
severity: should_fix
---

**Rule.** When defining C++ shim classes or wrapper factory functions created
exclusively for Rust FFI (e.g., in \*\_rust_shim.{h,cc} or \*\_shim.{h,cc}),
suffix the shim types and constructor helpers with 'Rs' (e.g.,
ScopedFeatureListRs, CreateScopedFeatureListRs) and enclose them within a
dedicated 'rust' or 'ffi' sub-namespace (such as base::test::rust or
content::rust). Do not place Rust FFI shim types directly in general-purpose
namespaces or use generic names like \*Shim without a language prefix/suffix.

**Why.** C++ shim classes created specifically for Rust FFI bindings can be
mistaken for general-purpose C++ utilities or collide with existing C++ types if
placed in standard namespaces with generic names like 'Shim'. Suffixing type
names with 'Rs' and scoping them within a dedicated 'rust' or 'ffi'
sub-namespace clearly signals that the class is an FFI adapter intended solely
for Rust interop.

## Examples

Bad:

```cpp
// base/test/scoped_feature_list_rust_shim.h
namespace base::test {

// Bad: Generic name 'Shim' in a public namespace looks like a general C++ utility.
class ScopedFeatureListShim {
 public:
  ScopedFeatureListShim();
  void InitFromCommandLine(::rust::Str enable_features,
                           ::rust::Str disable_features);
 private:
  ScopedFeatureList scoped_feature_list_;
};

std::unique_ptr<ScopedFeatureListShim> CreateScopedFeatureList();

}  // namespace base::test
```

Good:

```cpp
// base/test/scoped_feature_list_rust_shim.h
namespace base::test::rust {

// Good: Isolated in a 'rust' sub-namespace with 'Rs' type suffix.
class ScopedFeatureListRs {
 public:
  ScopedFeatureListRs();
  void InitFromCommandLine(::rust::Str enable_features,
                           ::rust::Str disable_features);
 private:
  ScopedFeatureList scoped_feature_list_;
};

std::unique_ptr<ScopedFeatureListRs> CreateScopedFeatureListRs();

}  // namespace base::test::rust
```
