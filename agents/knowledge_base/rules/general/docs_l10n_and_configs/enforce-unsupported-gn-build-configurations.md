---
name: general/enforce-unsupported-gn-build-configurations
rubric: correctness
severity: should_fix
---

**Rule.** When a feature, build argument, or config combination is unsupported
in certain build configurations (such as component builds, specific sanitizer
modes, or non-target operating systems), enforce the constraint using a GN-level
`assert(condition, "error message")` in the defining `.gni` or `BUILD.gn` file.
Do not silently omit required configurations or rely on downstream compilation
failures to surface invalid build arguments.

**Why.** Without an explicit GN assertion, unsupported feature-flag and
build-configuration combinations may silently omit required configs or produce
confusing compilation and link-time failures. Enforcing constraints via assert()
guarantees immediate, actionable diagnostics at `gn gen` time.

## Examples

Bad:

```cpp
# In partition_alloc/BUILD.gn:
if (is_component_build) {
  # alloc token is only supported on Linux in component builds.
  if (is_linux) {
    public_configs += [ "//build/config/sanitizers:alloc_token" ]
  }
}
```

Good:

```cpp
# In partition_alloc.gni:
assert(!is_component_build || is_linux || !shim_supports_alloc_token,
       "In the component build, alloc token is only supported on Linux.")
```
