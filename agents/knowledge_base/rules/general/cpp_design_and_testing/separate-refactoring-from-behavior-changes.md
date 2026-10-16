---
name: general/separate-refactoring-from-behavior-changes
rubric: change_scope
severity: should_fix
---

**Rule.** Separate mechanical refactoring, modernization, and cleanups from
functional behavior changes into distinct, independent CLs. Additionally, split
distinct, independent functional changes—such as unrelated features, separate
presubmit checks, or changes targeting different parts of a workflow—into
dedicated, focused CLs rather than bundling them into a single change list. Do
not mix unrelated drive-by cleanups, dead-code deletions, or secondary feature
additions into a CL focused on a targeted fix or capability.

**Why.** CLs should effect only one logical change. Keeping refactorings
behavior-preserving allows reviewers to quickly verify them as move- or
syntax-only changes and prevents unnecessary reverts of cleanups if a behavior
change causes regressions. Isolating independent bug fixes, features, and
validation checks minimizes reviewer cognitive load and makes changes easier to
review, test, bisect, cherry-pick, or revert independently.

## Examples

Bad:

```cpp
// Combining mechanical modernization with functional bug fixes:
// [base] Modernize and optimize base::EnumSet
// - Marked EnumSet and Iterator methods as constexpr.
// - Fixed a bug in FromRange() where indices >= 64 cause a crash.

// OR bundling distinct, independent functional checks or features:
// testing/variations/PRESUBMIT.py:
def CheckBaseFeatureParamMacro(...): ...
def CheckUndeclaredFeatureParams(...): ...
```

Good:

```cpp
// Split into focused, independent CLs:
// CL 1 (bug fix): Fix FromRange() crash for indices >= 64.
// CL 2 (behavior-preserving modernization): Modernize base::EnumSet to constexpr.

// Land distinct functional changes or presubmit checks in separate CLs:
// CL 1: Implement CheckBaseFeatureParamMacro() for macro syntax.
// CL 2: Implement CheckUndeclaredFeatureParams() for testing configs.
```
