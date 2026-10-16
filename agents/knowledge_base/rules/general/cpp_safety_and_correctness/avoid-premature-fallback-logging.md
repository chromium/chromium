---
name: general/avoid-premature-fallback-logging
rubric: correctness
severity: should_fix
---

**Rule.** Do not log that a fallback action or alternative path is occurring
before verifying the condition, feature flag, or runtime check controlling that
fallback. Emit fallback log messages only within the conditional branch where
the fallback actually executes, and ensure messages logged prior to evaluating
the condition describe only the failure or unavailability of the primary path.

**Why.** Emitting a log message that claims a fallback action is occurring
before evaluating the controlling condition produces misleading diagnostics when
the condition evaluates to false and the operation aborts or returns early
without executing the fallback.

## Examples

Bad:

```cpp
// Premature log claims fallback is occurring before checking the flag.
PLOG(WARNING) << "madvise not supported, falling back to fork.";
if (!features::kLibraryPrefetcherMadviseFallback.Get()) {
  return PrefetchStatus::kMadviseFailed;
}
return PrefetchWithFork(sections);
```

Good:

```cpp
// Log the failure reason first, then log the fallback inside the enabled branch.
PLOG(WARNING) << "madvise not supported for library prefetch";
if (!features::kLibraryPrefetcherMadviseFallback.Get()) {
  return PrefetchStatus::kMadviseFailed;
}
LOG(WARNING) << "falling back to fork-based library prefetch";
return PrefetchWithFork(sections);
```
