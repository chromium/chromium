---
name: general/encapsulate-repetitive-atomic-accessors
rubric: design
severity: should_fix
---

**Rule.** Encapsulate atomic operations (such as repetitive `.load()` or
`.store()` calls with explicit memory orderings) and low-level read-modify-write
retry loops (such as `compare_exchange_weak` bit-masking loops) into dedicated
helper functions or accessor methods rather than scattering or inlining them
inside higher-level business logic. Use these helpers at call sites to
centralize concurrency invariants and keep caller control flow clear.

**Why.** Inlining raw atomic calls or complex compare-and-swap retry loops mixes
abstraction levels, creates visual clutter, and obscures caller control flow.
Encapsulating synchronization and memory orderings in dedicated helpers provides
a single, well-documented location for concurrency invariants and keeps
higher-level logic readable.

## Examples

Bad:

```cpp
// Bad: Scattering raw atomic loads across multiple expressions
return (address & pool_mask_.load(std::memory_order_relaxed)) ==
       pool_base_.load(std::memory_order_relaxed);

// Bad: Inlining a low-level atomic CAS retry loop inside high-level business logic
FeatureList::OverrideState FeatureList::GetOverrideState(const Feature& feature) {
  OverrideState state = GetOverrideStateByFeatureName(feature.name);
  uint32_t new_val = PackFeatureCache(state, caching_context_);
  uint32_t expected = feature.cached_value.load(std::memory_order_relaxed);
  uint32_t to_store;
  do {
    to_store = new_val | (expected & (kLogGeneralMask | kLogEarlyMask));
  } while (!feature.cached_value.compare_exchange_weak(
      expected, to_store, std::memory_order_relaxed, std::memory_order_relaxed));
  return state;
}
```

Good:

```cpp
// Good: Encapsulate repetitive atomic operations in accessor methods
uintptr_t PoolBase() const { return pool_base_.load(std::memory_order_relaxed); }
uintptr_t PoolMask() const { return pool_mask_.load(std::memory_order_relaxed); }
// Call site:
return (address & PoolMask()) == PoolBase();

// Good: Extract atomic CAS retry loop into a dedicated helper function
void SetFeatureCachedBits(std::atomic<uint32_t>& cached_value, uint32_t new_val) {
  uint32_t expected = cached_value.load(std::memory_order_relaxed);
  uint32_t to_store;
  do {
    to_store = new_val | (expected & (kLogGeneralMask | kLogEarlyMask));
  } while (!cached_value.compare_exchange_weak(
      expected, to_store, std::memory_order_relaxed, std::memory_order_relaxed));
}
FeatureList::OverrideState FeatureList::GetOverrideState(const Feature& feature) {
  OverrideState state = GetOverrideStateByFeatureName(feature.name);
  SetFeatureCachedBits(feature.cached_value, PackFeatureCache(state, caching_context_));
  return state;
}
```
