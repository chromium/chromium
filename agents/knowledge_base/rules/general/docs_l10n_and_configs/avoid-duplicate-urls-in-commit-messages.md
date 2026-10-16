---
name: general/avoid-duplicate-urls-in-commit-messages
rubric: readability
severity: nit
---

**Rule.** Do not include duplicate URLs or redundant link entries in commit
message descriptions. When multiple modified strings, UI components, or changes
share the same translation screenshot, design doc, or reference link, list each
unique URL only once.

**Why.** Repeating identical URLs clutters the commit message body without
providing new information or context for reviewers, translators, or future
maintainers inspecting version control history.

## Examples

Bad:

```cpp
// Redundant duplicate screenshot URLs in commit message description:
Cloud links:
https://storage.cloud.google.com/chromium-translation-screenshots/adef98f54f64ee9e5da64b400398683f596f9c6c

https://storage.cloud.google.com/chromium-translation-screenshots/5a9f157235fcc018285825844e98c0e3eaabb59e

https://storage.cloud.google.com/chromium-translation-screenshots/adef98f54f64ee9e5da64b400398683f596f9c6c
```

Good:

```cpp
// Deduplicated unique URLs in commit message description:
Cloud links:
https://storage.cloud.google.com/chromium-translation-screenshots/adef98f54f64ee9e5da64b400398683f596f9c6c

https://storage.cloud.google.com/chromium-translation-screenshots/5a9f157235fcc018285825844e98c0e3eaabb59e
```
