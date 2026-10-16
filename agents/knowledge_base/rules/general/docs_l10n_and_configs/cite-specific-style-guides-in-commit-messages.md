---
name: general/cite-specific-style-guides-in-commit-messages
rubric: readability
severity: should_fix
---

**Rule.** When citing style guides, conventions, or coding standards in commit
messages or documentation, explicitly name the specific guide (such as 'The
Google TypeScript Style Guide', 'Chromium Web Development Style Guide', or
'styleguide/web/web.md') rather than ambiguously referring to 'our style guide'.

**Why.** Chromium relies on upstream style guides (such as the Google TypeScript
Style Guide) supplemented by project-specific overrides. Ambiguous references
like 'our style guide' create confusion about which standard or override governs
the code.

## Examples

Bad:

```cpp
Our typescript style guide says not to use trailing underscores, so I've removed them from this documentation.
```

Good:

```cpp
The Google TypeScript style guide says not to use trailing underscores, so I've removed them from this documentation.
```
