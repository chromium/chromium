---
name: general/document-failure-modes-for-compiler-workarounds
rubric: readability
severity: nit
---

**Rule.** When introducing compiler attributes, optimization overrides, or
toolchain pragmas (such as __attribute__((used)), NOINLINE, optnone, or #pragma
clang optimize off) to work around compiler behavior, inlining, or dead-code
elimination, add an inline comment documenting the specific failure mode,
toolchain, and build configuration (e.g., ThinLTO undefined symbol errors) that
necessitates the workaround.

**Why.** Optimization overrides and compiler attributes often appear redundant
or detrimental to code size and performance. Explaining the exact failure
mechanism and build context ensures future maintainers understand why the
workaround is load-bearing and prevents accidental regressions from cleanup
refactorings.

## Examples

Bad:

```cpp
// Also the `used` attribute is required to prevent dead-code elimination.
#define SHIM_ALWAYS_EXPORT __attribute__((used, visibility("default"), noinline))
```

Good:

```cpp
// Also the `used` attribute is required to prevent dead-code elimination.
// Without it, LLVM LTO can cause an undefined symbol error, even if the symbol
// is defined within liballocator_shim.a.
#define SHIM_ALWAYS_EXPORT __attribute__((used, visibility("default"), noinline))
```
