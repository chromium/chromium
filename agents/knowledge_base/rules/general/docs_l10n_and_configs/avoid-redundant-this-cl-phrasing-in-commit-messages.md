---
name: general/avoid-redundant-this-cl-phrasing-in-commit-messages
rubric: readability
severity: nit
---

**Rule.** In CL descriptions and commit messages, avoid redundant filler phrases
such as 'This CL adds/changes...', 'This change...', or 'This patch...'. Start
sentences directly using imperative mood or active voice (e.g., 'Add foo' rather
than 'This CL adds foo').

**Why.** Phrases like 'This CL' or 'This change' add unnecessary boilerplate to
commit descriptions. Writing directly in the imperative mood keeps commit logs
concise, direct, and easier to scan in git log and code review tools.

## Examples

Bad:

```cpp
This CL adds a new UI switch in chrome://settings/system for Process Isolation.

This change updates the preferences service to persist the new setting.
```

Good:

```cpp
Add a new UI switch in chrome://settings/system for Process Isolation.

Update the preferences service to persist the new setting.
```
