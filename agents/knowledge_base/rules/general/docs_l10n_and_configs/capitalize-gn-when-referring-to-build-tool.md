---
name: general/capitalize-gn-when-referring-to-build-tool
rubric: readability
severity: nit
---

**Rule.** When referring to the build tool by name in documentation, prose, or
code comments, capitalize 'GN' (Generate Ninja). Reserve lowercase,
code-formatted `gn` (e.g., in backticks or code blocks) for executable
command-line invocations (`gn gen`, `gn args`) and file extensions (`.gn`,
`.gni`).

**Why.** 'GN' is a proper noun and acronym for Generate Ninja. Standardizing on
uppercase 'GN' for the tool name while reserving code-formatted lowercase `gn`
for CLI commands and file extensions prevents ambiguity and maintains
consistency across documentation and code comments.

## Examples

Bad:

```cpp
* Avoid C preprocessor conditionals. Use runtime logic or build-time gn conditionals, e.g. `if (is_win)`.
* gn variables are allowed, but cannot be overridden by `gn args`.
```

Good:

```cpp
* Avoid C preprocessor conditionals. Use runtime logic or build-time GN conditionals, e.g. `if (is_win)`.
* GN variables are allowed, but cannot be overridden by `gn args`.
```
