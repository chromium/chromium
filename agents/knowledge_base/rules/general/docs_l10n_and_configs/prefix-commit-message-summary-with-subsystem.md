---
name: general/prefix-commit-message-summary-with-subsystem
rubric: readability
severity: should_fix
---

**Rule.** Prefix or qualify the commit message summary line with the affected
subsystem, component, directory (such as 'Extensions:', 'views:', or 'base:'),
or target scope under 72 columns, and state the concrete change or mechanism
introduced (using specific imperative verbs like 'Introduce', 'Add', 'Refactor',
or 'Support'). Do not end the summary line with a period or terminal
punctuation. Do not describe symptoms, test failures, or crashes in the subject
line, and avoid vague buzzwords like 'modernize' or 'clean up'. Additionally,
avoid Conventional Commits prefixes (such as 'Fix:', 'fix(...)', or 'feat:') and
do not use platform-specific tags (such as '[iOS]' or '[Android]') on
platform-agnostic shared code; reserve detailed technical rationale, symptoms,
and motivation for the description body.

**Why.** Commit message summary lines function as headline titles and are
displayed in 'git log --oneline', Gerrit change lists, email subjects, and
bisect logs where file paths and full descriptions are not visible. Specifying
the target subsystem and concrete mechanism conveys what behavior changed at a
glance, while headline summaries omit terminal punctuation to maximize readable
space and maintain consistent formatting. Conversely, describing symptoms or
failures obscures the actual fix, subjective buzzwords ('modernize') fail to
convey what was modified, Conventional Commits prefixes consume character budget
without adding subsystem context, and platform tags on shared code mislead
reviewers.

## Examples

Bad:

```cpp
// Missing subsystem prefix, rationale in subject, and trailing period:
Use base::StrCat to avoid unnecessary string copies.

// Conventional Commits prefix and vague buzzword:
Fix: Modernize tab context menu with inline device submenu

// Subject line describes symptom/failure rather than mechanism:
Fix test failures caused by loading native library early

// Platform prefix on platform-agnostic shared code:
[iOS] Introduce RunCurrentlyNonDelayedTasks

// Trailing period on headline summary:
Create synthetic field trial to skip pre-reading dlls on Windows.
```

Good:

```cpp
// Subsystem prefix, concrete action, no trailing period, rationale in body:
base: Use base::StrCat() in gtest_util

`base::StrCat` avoids unnecessary intermediate string copies.

// Subsystem prefix and specific descriptive verb:
[STTS] Introduce tab context menu with inline device submenu

// Concrete mechanism instead of symptom description:
Make DeviceInfo.setForTesting methods apply to native code

Loading native library during early startup previously bypassed overrides.

// Platform-agnostic shared code without platform tag:
base: Introduce RunCurrentlyNonDelayedTasks

// Imperative summary without trailing period:
Create synthetic field trial to skip pre-reading dlls on Windows
```
