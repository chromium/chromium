---
name: general/document-callback-and-virtual-method-contracts
rubric: readability
severity: should_fix
---

**Rule.** When documenting classes, interfaces, virtual methods, or callbacks,
describe the entity's own public contract, parameter semantics, and threading
constraints without coupling comments to specific caller classes or asserting
unconditional caller actions. For methods returning callbacks with sequence or
threading constraints, explicitly state whether restrictions apply to calling
the factory method or invoking the returned callback. For internal private
methods, describe what the code does (e.g., 'are executed on...') rather than
phrasing internal mechanics as external caller requirements ('must be executed
on...').

**Why.** Public contracts and callback doc comments should clearly state the
abstraction's own expectations, preconditions, and guarantees. Ambiguous
subjectless comments leave callers uncertain whether sequence restrictions
govern factory invocation or callback execution, while coupling comments to
callers or misrepresenting internal synchronization obscures component
boundaries and contracts.

## Examples

Bad:

```cpp
// Variant 1: Coupling interface method comments to a specific caller.
// RecordDelta is used by HistogramSnapshotManager to handles the logistics.
virtual void RecordDelta(const HistogramBase& histogram,
                         const HistogramSamples& snapshot) = 0;

// Variant 2: Asserting external caller actions as class guarantees.
// Whenever a worker wakes up, it will call OnWorkerThreadActive to sample.
void OnWorkerThreadActive();

// Variant 3: Ambiguous constraint on method returning a callback.
// NOTE: Must be called on the same sequence, or bound via BindPostTask.
base::RepeatingCallback<void(int)> GetCallback();

// Variant 4: Phrasing internal execution context as a caller requirement.
// All the private functions below must be executed on task runner.
void InternalSync();
```

Good:

```cpp
// Variant 1: Describing the interface method's own contract and arguments.
// Called for each histogram with a `snapshot` of the new samples (delta).
virtual void RecordDelta(const HistogramBase& histogram,
                         const HistogramSamples& snapshot) = 0;

// Variant 2: Phrasing external interactions conditionally.
// If a worker thread becomes active (via OnWorkerThreadActive), it is sampled.
void OnWorkerThreadActive();

// Variant 3: Explicitly stating that constraint applies to returned callback.
// NOTE: The returned callback must be called on the same sequence as this
// object, or bound via BindPostTask.
base::RepeatingCallback<void(int)> GetCallback();

// Variant 4: Describing internal implementation behavior in present tense.
// All the private functions below are executed on the task runner.
void InternalSync();
```
