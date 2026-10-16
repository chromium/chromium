---
name: general/use-ascii-quotes-in-markdown-documentation
rubric: readability
severity: nit
---

**Rule.** In Markdown documentation (`*.md`), use standard ASCII straight
apostrophes and quotation marks (`'`, `"`) rather than Unicode typographic or
smart curly quotes (`’`, `‘`, `“`, `”`, `„`). Convert curly quotes and
apostrophes copied from rich-text editors (such as Google Docs) to ASCII
equivalents, as `git cl format` does not format or normalize Markdown files.

**Why.** Text copied from external rich-text editors often introduces
typographic curly quotes and apostrophes. Using ASCII quotation marks ensures
consistent typography across all documentation, prevents character encoding or
rendering glitches in terminal pagers and plain-text viewers, and keeps text
directly searchable using standard grep and codesearch queries.

## Examples

Bad:

```cpp
It’s useful if any of the following applies:
* We have disabled Chromium’s formatter in the `third_party/` directory
* Run the tool with “--dry-run” mode first
```

Good:

```cpp
It's useful if any of the following applies:
* We have disabled Chromium's formatter in the `third_party/` directory
* Run the tool with "--dry-run" mode first
```
