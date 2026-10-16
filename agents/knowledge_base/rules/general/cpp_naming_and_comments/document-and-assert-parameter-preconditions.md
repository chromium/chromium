---
name: general/document-and-assert-parameter-preconditions
rubric: readability
severity: should_fix
---

**Rule.** When a function, method, constructor, setter, or initialization
routine has parameter constraints, range limits, initialization ordering,
idempotency limits, or lifecycle state preconditions (such as being callable
only when stopped or not running, only after dependency initialization, or only
once) not enforced by types, document the contract in the header doc comment and
enforce it at entry using CHECK, DCHECK, or relational assertions (e.g.,
CHECK(!IsRunning()), DCHECK_GE). Conversely, do not retain or add comment-based
preconditions when the required invariants are already codified and guaranteed
by parameter types or wrapper classes.

**Why.** Implicit parameter assumptions, unasserted lifecycle preconditions, and
undocumented initialization ordering or idempotency constraints cause API
misuse, unexpected reinitialization, and hard-to-debug crashes downstream.
Documenting calling preconditions in headers clarifies the contract and
asserting them fails fast on violations, while omitting comments on strongly
typed parameters avoids redundant documentation noise.

## Examples

Bad:

```cpp
// Variant 1: Undocumented / unasserted parameter constraint
// Missing documentation or CHECK for power-of-two capacity:
void SetBucketCount(size_t bucket_count) {
  bucket_count_ = bucket_count;
}

// Variant 2: Undocumented / unasserted lifecycle state precondition
// Missing precondition doc comment and CHECK(!IsRunning()):
void WallClockTimer::SetTaskRunner(
    scoped_refptr<SequencedTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  timer_.SetTaskRunner(std::move(task_runner));
}

// Variant 3: Undocumented initialization ordering and idempotency constraints
static void InitializeFeatures();

// Variant 4: Redundant precondition comments on strongly-typed APIs
// PRECONDITIONS: 'env' must be a ParsedNativeEnvironmentString.
NativeEnvironmentString AlterEnvironment(
    const ParsedNativeEnvironmentString& env,
    const EnvironmentMap& changes);
```

Good:

```cpp
// Variant 1: Documented and asserted parameter precondition
// |bucket_count| must be a non-zero power of two.
void SetBucketCount(size_t bucket_count) {
  CHECK(std::has_single_bit(bucket_count));
  bucket_count_ = bucket_count;
}

// Variant 2: Documented and asserted lifecycle state precondition
// This method can only be called while the timer isn't running.
void WallClockTimer::SetTaskRunner(
    scoped_refptr<SequencedTaskRunner> task_runner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!IsRunning());
  timer_.SetTaskRunner(std::move(task_runner));
}

// Variant 3: Documented initialization ordering and idempotency constraints
// Applies features to base::Lock. Must be called on the main thread only
// after the feature list has been initialized and only once.
static void InitializeFeatures();

// Variant 4: Relying on parameter types for invariants without comment noise
NativeEnvironmentString AlterEnvironment(
    const ParsedNativeEnvironmentString& env,
    const EnvironmentMap& changes);
```
