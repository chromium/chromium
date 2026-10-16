---
name: general/verify-shutdown-or-cleanup-in-destructor
rubric: correctness
severity: nit
---

**Rule.** When a class requires an explicit cleanup or Shutdown() method to be
called before destruction, assert that condition in the destructor using CHECK
(or DCHECK) on the cleared state or shutdown flag. Do not leave the destructor
defaulted (= default) or empty when uncleared state would violate the lifecycle
contract.

**Why.** Asserting in the destructor that explicit shutdown or cleanup occurred
catches improper lifecycle management early, ensuring lifecycle contract
violations fail fast rather than causing subtle leaks, data races, or
use-after-free bugs during teardown.

## Examples

Bad:

```cpp
ThreadGroupProfiler::~ThreadGroupProfiler() = default;
```

Good:

```cpp
ThreadGroupProfiler::~ThreadGroupProfiler() {
  // Shutdown() must be called prior to destruction.
  CHECK(!active_collection_);
}
```
