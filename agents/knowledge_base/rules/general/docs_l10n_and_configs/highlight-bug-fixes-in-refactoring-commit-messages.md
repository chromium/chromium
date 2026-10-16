---
name: general/highlight-bug-fixes-in-refactoring-commit-messages
rubric: readability
severity: should_fix
---

**Rule.** When fixing a functional bug or pre-existing logic defect alongside
automated or mechanical refactorings (such as spanification, API migrations, or
cleanups), explicitly highlight and describe the bug fix in the commit subject
and description body rather than describing the CL solely as a refactoring.

**Why.** Reviewers, triage bots, and engineers investigating regressions or
bisecting history rely on commit messages to identify behavioral changes.
Subsuming a functional fix into a mechanical refactoring without documenting it
obscures the behavioral change and hinders regression tracking, triage, and
cherry-picking.

## Examples

Bad:

```cpp
spanification of libyuv_scale_fuzzer.cc

This is the result of running the automatic spanification on linux and
updating code to use and pass spans where size is known.
```

Good:

```cpp
spanification of libyuv_scale_fuzzer.cc and advance of RNG state preservation.

The random number generator state is passed as a copy to
FillBufferWithRandomData, so the outer RNG is never advanced, making the
three planes populated with the exact same sequence of random numbers and
reducing fuzzer effectiveness. With this change the RNG is passed as a
reference and its state is preserved and advanced as expected.
```
