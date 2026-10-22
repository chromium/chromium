---
name: context-hub/mojo-runtime-feature-gating
rubric: correctness
severity: blocking
---

**Rule.** Annotate new sub-feature Mojo interfaces and methods in
`context_hub.mojom` with `[RuntimeFeature=...]` using the corresponding
sub-feature flag (`kAutoTabGroups`, `kAutoTodos`, `kRecordReplay`,
`kSmartSearch`, `kTopics`, `kMemoryBanksSearchPalette`).

**Why.** Gating sub-feature Mojo methods with `[RuntimeFeature=...]` at the
interface boundary prevents disabled sub-features from being invoked over IPC
during staged rollouts and Finch experiments.

## Examples

Bad:

```mojom
// context_hub.mojom: missing [RuntimeFeature] on a sub-feature method.
interface PageHandler {
  DeleteTopic(string topic_id) => (bool success);
};
```

Good:

```mojom
// context_hub.mojom: gated with [RuntimeFeature] at the Mojo boundary.
interface PageHandler {
  [RuntimeFeature=browser.context_hub.mojom.kTopics]
  DeleteTopic(string topic_id) => (bool success);
};
```
