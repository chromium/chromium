---
name: general/format-code-identifiers-in-backticks-in-markdown
rubric: readability
severity: nit
---

**Rule.** In Markdown documentation and public changelogs, format code
identifiers, symbols, and Web API types in inline backticks (e.g.,
`InteractionContentfulPaint`). Use exact, official interface and type names
(e.g., `SoftNavigationEntry`) rather than colloquial shorthand or ad-hoc
acronyms (e.g., do not coin `ICP` for `InteractionContentfulPaint` or
`SoftNavigation` for `SoftNavigationEntry`).

**Why.** Formatting code identifiers in inline backticks visually distinguishes
code elements from surrounding narrative text. Using full, official interface
names rather than ad-hoc acronyms or truncated shorthand avoids ambiguity and
prevents misleading external developers and analytics providers into believing a
new official Web Vital or metric is being launched.

## Examples

Bad:

```cpp
* Metric bug fix: Use correct NavigationId for ICP entries
* **InteractionContentfulPaint decoupled from SoftNavigation and reported for all Interactions**.
* Added `interactionId` attribute to ICP and SoftNavigation performance entry ([704c0222f53a3](https://chromium-review.googlesource.com/c/chromium/src/+/7638455)).
```

Good:

```cpp
* Metric bug fix: Use correct `NavigationId` for `InteractionContentfulPaint` entries
* **`InteractionContentfulPaint` decoupled from `SoftNavigationEntry` and reported for all interactions**.
* Added `interactionId` attribute to `InteractionContentfulPaint` and `SoftNavigationEntry` performance entries ([704c0222f53a3](https://chromium-review.googlesource.com/c/chromium/src/+/7638455)).
```
