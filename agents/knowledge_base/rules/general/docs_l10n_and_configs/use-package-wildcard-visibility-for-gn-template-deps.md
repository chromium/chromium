---
name: general/use-package-wildcard-visibility-for-gn-template-deps
rubric: design
severity: should_fix
---

**Rule.** When defining internal helper targets (such as C++ shims, bridge
targets, or private implementation source sets) in GN build files (`BUILD.gn`
and `*.gni`), explicitly restrict their visibility using package wildcards
(`visibility = [ ":*" ]` or `visibility = [ "//dir:*" ]`) so external targets
cannot depend on them directly. Do not restrict visibility to a single target
name (e.g., `visibility = [ ":target" ]`), as this breaks GN templates (like
`fuzzer_test`, Rust libraries, or Android targets) that generate internal
subtargets.

**Why.** By default, GN targets have unrestricted visibility, allowing external
modules to accidentally take dependencies on internal implementation details.
Restricting visibility to directory wildcards (`":*"`) enforces encapsulation
while ensuring GN templates that expand into internal intermediate subtargets
(such as test harnesses, companion libraries, or clippy actions) can still
resolve their dependencies.

## Examples

Bad:

```cpp
# BAD: Internal helper target has unrestricted visibility by default
source_set("feature_shim") {
  sources = [
    "feature_rust_shim.cc",
    "feature_rust_shim.h",
  ]
  deps = [ "//base" ]
}

# BAD: Restricting visibility to a single target name breaks template subtargets
static_library("delegate_for_fuzzing") {
  visibility = [ ":presentation_service_mojolpm_fuzzer" ]
  sources = [ "delegate_for_fuzzing.cc" ]
  deps = [ "//base" ]
}
```

Good:

```cpp
# GOOD: Restrict internal helper to package/directory visibility
source_set("feature_shim") {
  visibility = [ ":*" ]
  sources = [
    "feature_rust_shim.cc",
    "feature_rust_shim.h",
  ]
  deps = [ "//base" ]
}

# GOOD: Package wildcard visibility allows all template subtargets
static_library("delegate_for_fuzzing") {
  visibility = [ ":*" ]
  sources = [ "delegate_for_fuzzing.cc" ]
  deps = [ "//base" ]
}
```
