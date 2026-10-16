---
name: general/refer-to-planned-changes-as-follow-up-cl-in-commit-messages
rubric: readability
severity: should_fix
---

**Rule.** Do not include tentative, speculative, or unconfirmed follow-up work
(such as claiming a feature or asset will be added in the next CL) in commit
messages; keep the description focused on what this change actually
accomplishes. When describing confirmed subsequent changes, refer to them as 'a
follow-up CL' (or 'a follow-up') rather than 'a following CL', and use direct
third-person phrasing (e.g., 'A follow-up CL will...') instead of conversational
first-person descriptions.

**Why.** Commit messages become part of permanent version control history.
Speculative claims about unconfirmed future changes become inaccurate or
misleading if exploratory work is abandoned or delayed, while using standard
third-person 'follow-up CL' phrasing keeps commit logs concise, factual, and
searchable.

## Examples

Bad:

```cpp
Animations will be added in next CL.

In a following CL we will use IsPseudoHandle() in base/win/scoped_handle.h.
```

Good:

```cpp
A follow-up CL will use IsPseudoHandle() in base/win/scoped_handle.h.
```
