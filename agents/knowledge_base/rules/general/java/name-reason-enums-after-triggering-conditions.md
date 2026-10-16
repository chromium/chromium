---
name: general/name-reason-enums-after-triggering-conditions
rubric: readability
severity: should_fix
---

**Rule.** When defining enumerators for reason, eviction, failure, or error
enums, name values after the specific triggering condition or event rather than
a broad feature or subsystem name alone. Ensure the identifier clearly describes
why the event occurred so readers and telemetry do not mistake the reason for
general feature usage.

**Why.** Naming a reason or failure enum value solely after a feature name (such
as kSharedWorker) misleadingly implies that merely using the feature triggers
the event or eviction. Naming the specific condition (such as
kSharedWorkerWithNoActiveClient) distinguishes distinct failure causes and
prevents misinterpreting telemetry.

## Examples

Bad:

```cpp
// Misleadingly named after the feature alone:
enum EvictionReason {
  SHARED_WORKER = 69;
};

// In C++:
enum class EvictionReason {
  kSharedWorker = 73,
};
```

Good:

```cpp
// Explicitly names the triggering condition:
enum EvictionReason {
  SHARED_WORKER_WITH_NO_ACTIVE_CLIENT = 69;
};

// In C++:
enum class EvictionReason {
  kSharedWorkerWithNoActiveClient = 73,
};
```
