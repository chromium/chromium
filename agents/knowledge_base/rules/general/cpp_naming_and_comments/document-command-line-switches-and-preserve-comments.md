---
name: general/document-command-line-switches-and-preserve-comments
rubric: readability
severity: should_fix
---

**Rule.** When declaring, defining, or relocating command-line switch constants
or configuration flags (such as inline constexpr char k...[] in namespace
switches or header files), include an explanatory comment describing what the
switch controls, its accepted arguments, or referencing flag_descriptions. When
moving or converting switches between files (such as moving from .cc to .h),
preserve existing explanatory comments rather than omitting them.

**Why.** Command-line switches alter runtime behavior and subsystem
configuration across processes. Documenting switches in-place ensures
maintainers understand their purpose and effects without tracing call sites, and
preserving comments during refactoring prevents institutional knowledge from
being lost.

## Examples

Bad:

```cpp
namespace switches {

// Variant 1: Undocumented command-line switch
inline constexpr char kEnableCustomFeature[] = "enable-custom-feature";

// Variant 2: Comment dropped when relocating switch to header
inline constexpr char kEnableBenchmarking[] = "enable-benchmarking";

}  // namespace switches
```

Good:

```cpp
namespace switches {

// Enables custom experimental feature for performance evaluation.
inline constexpr char kEnableCustomFeature[] = "enable-custom-feature";

// See flag_descriptions.cc for more details.
inline constexpr char kEnableBenchmarking[] = "enable-benchmarking";

}  // namespace switches
```
