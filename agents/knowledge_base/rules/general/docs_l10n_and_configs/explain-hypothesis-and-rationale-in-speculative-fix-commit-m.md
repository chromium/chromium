---
name: general/explain-hypothesis-and-rationale-in-speculative-fix-commit-m
rubric: readability
severity: should_fix
---

**Rule.** When authoring speculative bug fixes, test deflaking changes, or
non-obvious issue resolutions, include an explanatory paragraph in the commit
message body detailing why the change is being made, the suspected root cause or
failure mechanism, and the hypothesis for why the fix will be effective.

**Why.** A bare subject line or issue reference provides insufficient diagnostic
context for reviewers to evaluate speculative fixes and prevents gardeners or
future maintainers from effectively triaging regressions if the issue recurs or
flakes persist.

## Examples

Bad:

```cpp
Speculative fix to deflake testEnabledParentalControlsClosesIncognitoTabs

Bug: 462818064
```

Good:

```cpp
Speculative fix to deflake testEnabledParentalControlsClosesIncognitoTabs

Opening a regular tab while there is an incognito window doesn't always
focus the new tab. This CL manually brings the regular tab's task to
the front to ensure focus before proceeding.

Bug: 462818064
```
