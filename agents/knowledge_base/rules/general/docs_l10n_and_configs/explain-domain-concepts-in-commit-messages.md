---
name: general/explain-domain-concepts-in-commit-messages
rubric: readability
severity: should_fix
---

**Rule.** When authoring CL descriptions that introduce new architectures,
frameworks, or domain-specific terminology (such as novel subsystem
abstractions, workflows, or roles), explicitly define and explain the concepts
and contrast them with legacy models rather than assuming familiarity. Ensure
the description conveys why the concept is needed and how it fits into the
broader architecture.

**Why.** CL descriptions form permanent repository history used by reviewers and
future maintainers to understand design intent. Using unexplained jargon or
novel domain concepts without definition makes the architectural motivation
opaque and hinders historical code archaeology and regression triage.

## Examples

Bad:

```cpp
This CL introduces a new helper API `base::MultiMemoryConsumer` to allow a single class to cleanly support multiple memory interventions.
```

Good:

```cpp
MemoryConsumer is designed to replace MemoryPressureListener. In the legacy model, the global nature of memory pressure allowed a single listener to handle multiple unrelated actions (e.g., clearing a cache and triggering GC) from a single callback. In the Memory Coordinator model, however, interventions are granular and can be triggered individually. Since MemoryConsumer uses traits to describe how specific memory is managed, a class performing multiple unrelated interventions needs to register them as separate entities so they can be individually targeted and accounted for by the coordinator.
```
