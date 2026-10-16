---
name: general/prefer-positive-boolean-names-in-gn
rubric: readability
severity: optional
---

**Rule.** In GN build files (`.gn`, `.gni`), template definitions, and
`declare_args()` blocks, name boolean options, parameters, and flags positively
(such as `enable_foo` or `is_<feature>_compatible`) with inverted defaults
rather than using negative or exclusionary names (such as `disable_foo`,
`exclude_<feature>`, or `no_<feature>`). Maintain consistent naming conventions
across peer template parameters and target configurations.

**Why.** Negative and exclusionary boolean flags (such as `disable_foo = false`
or `exclude_wrapper = true`) are unintuitive to reason about, lead to confusing
double negatives in conditional checks, and violate GN naming conventions (which
recommend `enable_foo` over `disable_foo`). Using positive flag names with
appropriate default values keeps build configuration clear and consistent across
targets.

## Examples

Bad:

```cpp
# GN template parameter definition:
# - exclude_wrapper - do not apply the llvm wrapper for centipede builds

template("fuzzer_test") {
  if (!defined(invoker.exclude_wrapper) || !invoker.exclude_wrapper) {
    # Apply wrapper
  }
}

fuzzer_test("empty_fuzzer") {
  additional_configs = [ "//testing/libfuzzer:no_clusterfuzz" ]
  exclude_wrapper = true
}
```

Good:

```cpp
# GN template parameter definition:
# - is_fuzztest_compatible - whether the fuzzer is compatible with the fuzztest
#   wrapper when building with centipede. Defaults to true.

template("fuzzer_test") {
  if (!defined(invoker.is_fuzztest_compatible) ||
      invoker.is_fuzztest_compatible) {
    # Apply wrapper
  }
}

fuzzer_test("empty_fuzzer") {
  additional_configs = [ "//testing/libfuzzer:no_clusterfuzz" ]
  is_fuzztest_compatible = false
}
```
