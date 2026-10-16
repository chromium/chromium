---
name: general/reuse-existing-test-support-targets
rubric: design
severity: nit
---

**Rule.** Avoid creating dedicated, single-source build targets (such as
android_library, generate_jni, or source_set) for individual test utilities when
an existing shared test support target exists in the component. Add the source
files directly to the existing component-level test support targets (such as
:test_support, :<component>\_java_test_support, or :test_support_jni_headers).

**Why.** Creating fine-grained targets for individual test utilities
unnecessarily fragments the build graph, increases GN and Ninja configuration
overhead, and forces downstream test suites to manage redundant dependencies.

## Examples

Bad:

```cpp
generate_jni("scoped_feature_list_test_utils_jni_headers") {
  sources = [ "android/java/src/org/chromium/base/ScopedFeatureListTestUtils.java" ]
}

android_library("scoped_feature_list_test_utils_java") {
  srcjar_deps = [ ":scoped_feature_list_test_utils_jni_headers" ]
  deps = [ "//third_party/jni_zero:jni_zero_java" ]
  sources = [ "android/java/src/org/chromium/base/ScopedFeatureListTestUtils.java" ]
}
```

Good:

```cpp
generate_jni("test_support_jni_headers") {
  testonly = true
  sources = [
    "android/java/src/org/chromium/base/ScopedFeatureListTestUtils.java",
    # ...
  ]
}

android_library("base_java_test_support") {
  testonly = true
  srcjar_deps = [ ":test_support_jni_headers" ]
  sources = [
    "test/android/java/src/org/chromium/base/ScopedFeatureListTestUtils.java",
    # ...
  ]
}
```
