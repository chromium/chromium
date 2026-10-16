---
name: general/split-preparatory-test-fixes-into-separate-cl
rubric: change_scope
severity: should_fix
---

**Rule.** Split preparatory test fixes and test disables across the codebase
into an independent CL rather than bundling them into a large feature or reland
CL. Land the test adjustments in a preceding CL so they are validated
independently and linked from the main CL description.

**Why.** Bundling test fixes and disables with a large feature or reland CL
bloats change scope, complicates code review, and makes regressions harder to
bisect. Landing test fixes separately ensures they remain in the codebase even
if the main feature needs to be reverted.

## Examples

Bad:

```cpp
// Bundling widespread test updates and disables together with core feature changes in a single large reland CL:
// Patchset contains:
// - Core feature implementation in //components/...
// - Dozens of test fixes and DISABLED_ test annotations across //chrome/browser/...
```

Good:

```cpp
// 1. First CL: Land preparatory test updates and test disables:
//    "Fix and disable failing browser tests ahead of Feature X reland"
//
// 2. Second CL: Land the focused feature or reland CL linking the preparatory change:
//    "Reland Feature X (test fixes landed separately in crrev.com/c/12345)"
```
