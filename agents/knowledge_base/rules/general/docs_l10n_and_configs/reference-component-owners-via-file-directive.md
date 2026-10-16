---
name: general/reference-component-owners-via-file-directive
rubric: readability
severity: optional
---

**Rule.** In OWNERS files, delegate ownership of subsystem resources, strings,
or specific directories by referencing the subsystem's OWNERS file (e.g.,
`file://...` or `per-file ...=file://...`) rather than hardcoding individual
user emails. Referencing a component OWNERS file keeps review permissions
synchronized as team membership evolves.

**Why.** Hardcoding individual email addresses across multiple disparate OWNERS
files creates maintenance overhead and causes ownership lists to drift or become
stale over time. Referencing a single source OWNERS file via `file://` ensures
review coverage and ownership lists remain automatically in sync.

## Examples

Bad:

```cpp
file://chrome/browser/actor/OWNERS

haileywang@google.com
```

Good:

```cpp
file://chrome/browser/actor/OWNERS
file://chrome/browser/glic/android/OWNERS
```
