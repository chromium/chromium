---
name: general/assert-sequence-affinity-on-sequence-bound-methods
rubric: safety
severity: should_fix
---

**Rule.** In sequence-bound classes or objects configured during
pre-initialization, verify sequence affinity using
`DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_)` (or
`DCHECK_CALLED_ON_VALID_THREAD`) at the start of all public entry points and
state-accessing member methods. This includes mutating methods,
pre-initialization/configuration hooks, accessors, and functions vending
callbacks or `base::WeakPtr` instances. Do not inadvertently drop or omit
sequence assertions during refactoring or under the assumption that a method is
non-mutating or only called prior to initialization.

**Why.** Invoking member methods, configuration routines, or callback/WeakPtr
getters from unexpected sequences causes data races, thread safety violations,
and incorrect WeakPtr sequence binding. Consistently enforcing sequence affinity
on all entry points ensures threading contract violations fail fast in tests and
debug builds before subtle concurrency bugs emerge in production.

## Examples

Bad:

```cpp
// Bad: Omitting sequence checks on member methods, callback getters, or pre-init config.
void MemoryConsumerRegistry::RemoveMemoryConsumer(
    std::string_view consumer_name,
    MemoryConsumer* consumer) {
  uint32_t consumer_id = PersistentHash(consumer_name);
  OnMemoryConsumerRemoved(consumer_id, RegisteredMemoryConsumer(consumer));
}

base::RepeatingCallback<void(Types...)> GetRepeatingCallback() {
  return base::BindRepeating(&QueuedTestFuture::Push,
                             weak_ptr_factory_.GetWeakPtr());
}

void FeatureList::RegisterRuntimeMutableFeature(
    const Feature& feature, OnRuntimeOverrideStateChangedCallback callback) {
  DCHECK(!initialized_);
  DCheckOverridesAllowed();
}
```

Good:

```cpp
// Good: Asserting sequence affinity on public entry points, callback getters, and pre-init config.
void MemoryConsumerRegistry::RemoveMemoryConsumer(
    std::string_view consumer_name,
    MemoryConsumer* consumer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  uint32_t consumer_id = PersistentHash(consumer_name);
  OnMemoryConsumerRemoved(consumer_id, RegisteredMemoryConsumer(consumer));
}

base::RepeatingCallback<void(Types...)> GetRepeatingCallback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::BindRepeating(&QueuedTestFuture::Push,
                             weak_ptr_factory_.GetWeakPtr());
}

void FeatureList::RegisterRuntimeMutableFeature(
    const Feature& feature, OnRuntimeOverrideStateChangedCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(initialization_sequence_);
  DCHECK(!initialized_);
  DCheckOverridesAllowed();
}
```
