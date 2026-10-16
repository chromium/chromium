---
name: general/prefer-present-tense-in-comments
rubric: readability
severity: nit
---

**Rule.** In code comments and documentation, describe behavior, conditions, and
contracts in the present tense rather than the future tense (e.g., 'is
respected' instead of 'will be respected', or 'returns' instead of 'will
return').

**Why.** Comments and docstrings describe the current contract, state, and
guarantees of the code. Using present tense rather than future-tense
constructions ('will', 'will be') makes technical documentation direct, concise,
and active.

## Examples

Bad:

```cpp
// An override of `DIR_SYSTEM_TEMP` by tests will be respected.
// This function will return false if the handle is invalid.
```

Good:

```cpp
// An override of `DIR_SYSTEM_TEMP` by tests is respected.
// Returns false if the handle is invalid.
```
