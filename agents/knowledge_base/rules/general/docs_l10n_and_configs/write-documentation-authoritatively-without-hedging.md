---
name: general/write-documentation-authoritatively-without-hedging
rubric: readability
severity: nit
---

**Rule.** In technical documentation, cheat sheets, and reference guides, write
authoritatively and avoid tentative hedging phrases (such as 'it seems that',
'it appears that', 'probably', or 'we believe') when stating technical facts,
limitations, or API availability. State technical capabilities, requirements,
and absences directly.

**Why.** Hedging language creates unnecessary ambiguity and uncertainty for
readers looking for definitive API mappings, platform support, or technical
constraints. Authoritative statements provide clear and reliable guidance.

## Examples

Bad:

```cpp
macros, but it seems that there is no equivalents of
`DCHECK_GT`, `DCHECK_LE`, etc.
```

Good:

```cpp
macros, but no equivalents of `DCHECK_GT`, `DCHECK_LE`, etc.
```
