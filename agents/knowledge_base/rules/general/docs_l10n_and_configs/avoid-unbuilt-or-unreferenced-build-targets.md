---
name: general/avoid-unbuilt-or-unreferenced-build-targets
rubric: readability
severity: should_fix
---

**Rule.** Do not introduce new build targets (such as source_set,
static_library, or component) or source files in a CL if they are unreferenced
and not built anywhere in the repo. Land new code and targets in the CL where
they are wired into dependencies and actively consumed, or integrate the
implementation into existing built targets.

**Why.** Targets and source files that are not referenced or built anywhere in
the build graph are dead code. They are not compiled or exercised by trybots and
continuous integration, are prone to silent bitrot, and force reviewers to
assess code without the context of its callers and consumers.

## Examples

Bad:

```cpp
# In BUILD.gn: target is defined but not added to any deps or root build targets
if (is_android) {
  source_set("android_fuzzer_clean_exit") {
    sources = [ "android_fuzzer_clean_exit.cc" ]
    deps = [ "//base" ]
  }
}
```

Good:

```cpp
# Introduce the target or source file in the CL where it is wired up to its caller
if (is_android) {
  source_set("android_fuzzer_clean_exit") {
    sources = [ "android_fuzzer_clean_exit.cc" ]
    deps = [ "//base" ]
  }

  executable("fuzzer_runner") {
    sources = [ "fuzzer_runner.cc" ]
    deps = [ ":android_fuzzer_clean_exit" ]
  }
}
```
