---
name: general/conditionally-include-large-assets-in-gn-templates
rubric: safety
severity: should_fix
---

**Rule.** Do not unconditionally add large data, locale, or asset dependencies
(such as ICU data tables or heavy test fixtures) in shared GN templates that
instantiate multiple binary or package targets (such as test or fuzzer APKs).
Instead, conditionally include them by filtering `invoker.deps` (e.g. using
`filter_include(invoker.deps, [ ... ]) != []`) or providing an explicit opt-in
parameter so heavy assets are packaged only for targets that actually require
them.

**Why.** Unconditionally bundling large data or asset dependencies into shared
GN templates causes massive aggregate build artifact and package bloat when
multiplied across hundreds of generated targets (such as hundreds of test or
fuzzer APKs). Inspecting invoker dependencies ensures targets only package
assets they actually depend on.

## Examples

Bad:

```cpp
# In a shared template like fuzzer_test or test:
if (is_android) {
  # Anti-pattern: Unconditionally adds 9MB of ICU data to every fuzzer APK target.
  test_deps += [ "//third_party/icu:icu_assets" ]
}
```

Good:

```cpp
# In a shared template like fuzzer_test or test:
if (is_android) {
  _needs_icu = false
  if (defined(invoker.deps)) {
    _needs_icu = filter_include(invoker.deps,
                                [
                                  "*//third_party/icu*",
                                  "*//base:i18n*",
                                ]) != []
  }
  if (_needs_icu) {
    test_deps += [ "//third_party/icu:icu_assets" ]
  }
}
```
