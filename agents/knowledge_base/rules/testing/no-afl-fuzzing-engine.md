---
name: testing/no-afl-fuzzing-engine
rubric: design
severity: should_fix
---

**Rule.** Do not reference AFL (American Fuzzy Lop) as a supported fuzzing
engine or rely on `use_afl` in documentation and build configurations. AFL is
deprecated and unsupported on Chromium's ClusterFuzz infrastructure; use
FuzzTest for new fuzz targets or libFuzzer for maintaining legacy targets
instead.

**Why.** AFL is no longer supported by Chromium's automated fuzzing
infrastructure (ClusterFuzz), and the `use_afl` build flag and tryservers are
deprecated and broken. Referencing AFL misleads developers and maintainers into
configuring unsupported fuzzing workflows.

## Examples

Bad:

```cpp
*   [AFL]: Another popular fuzzing engine supported by our infrastructure.

# In BUILD.gn
if (use_afl) {
  fuzzer_engine = "//testing/libfuzzer:afl_driver"
}
```

Good:

```cpp
*   [libFuzzer]: An in-process, coverage-guided fuzzing engine used for legacy targets.
*   [FuzzTest]: Google's C++ fuzzing framework recommended for new fuzz targets.

# In BUILD.gn
if (use_fuzzing_engine) {
  # Configure supported fuzzing engines (FuzzTest or libFuzzer)
}
```
