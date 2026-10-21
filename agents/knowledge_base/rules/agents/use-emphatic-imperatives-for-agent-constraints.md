---
name: agents/use-emphatic-imperatives-for-agent-constraints
rubric: readability
severity: nit
---

**Rule.** When authoring guidance, rules, prompt constraints, or exclusion lists
intended for AI or LLM agents (such as non-bug classifications, forbidden
actions, or out-of-scope tasks), state negative boundaries using emphatic
imperative phrasing (such as 'Under no circumstances...', 'MUST NOT', or
'NEVER'). Avoid passive or mild declarative statements that models may interpret
as optional suggestions.

**Why.** AI and LLM agents frequently treat mild, passive, or declarative
statements as soft suggestions rather than strict operational boundaries. Using
explicit, emphatic imperative language firmly bounds agent behavior, prevents
false-positive reports, and ensures strict compliance with exclusion lists.

## Examples

Bad:

```cpp
## Non-Bugs
These are not security bugs, regardless of how easy they are to trigger.
```

Good:

```cpp
## Non-Bugs
These are not security bugs, regardless of how easy they are to trigger. Under no circumstances is something matching an entry in this list a security bug.
```
