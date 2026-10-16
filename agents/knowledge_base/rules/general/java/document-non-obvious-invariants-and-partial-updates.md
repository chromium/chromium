---
name: general/document-non-obvious-invariants-and-partial-updates
rubric: readability
severity: should_fix
---

**Rule.** When code relies on a non-obvious invariant, deliberately avoids an
apparent optimization (such as recomputing hashes for untrusted data), adheres
to standard library restrictions (such as avoiding throwing methods), implements
partial state updates, structural lifecycle asymmetries, out-of-band
configuration setters, manual state reconciliation, or test cleanups resetting
global state, add an inline comment documenting why the logic is necessary, how
invariants hold, and what failure or security vulnerability it prevents. For
unassigned global or singleton allocations (such as base::FieldTrialList),
explicitly cite the reset or cleanup method in comments.

**Why.** Subtle invariants, avoided optimizations, standard library
restrictions, partial state updates, out-of-band initialization setters,
unassigned singleton allocations, operational asymmetries, and non-obvious test
teardown hooks appear defective, unoptimized, or redundant without surrounding
context. Explanatory comments documenting sufficiency, security rationale,
architectural constraints, and teardown mechanisms prevent mistaken
refactorings, security flaws, linker errors, bugs, and memory leak concerns.

## Examples

Bad:

```cpp
// Variant 1: Partial sentinel update without sufficiency comment
#if PA_CONFIG(THREAD_CACHE_FAST_TLS)
  PA_UNSAFE_TODO(
      internal::g_thread_caches[internal::kThreadCacheTombstoneIndex]) =
      reinterpret_cast<ThreadCache*>(kTombstone);
#endif

// Variant 2: Unassigned global allocation with vague cleanup comment
// Create a FieldTrialList; this is not leaked since it will be deleted later.
new base::FieldTrialList();

// Variant 3: Deliberately avoided optimization without safety comment
// Calling FindHistogram without name_hash() looks like a missed optimization:
HistogramBase* existing = StatisticsRecorder::FindHistogram(lookup_name);

// Variant 4: Standard library member without comment on avoided throwing methods
std::bitset<kMaxSuperPagesInPool> alloc_bitset_ PA_GUARDED_BY(lock_);

// Variant 5: Test teardown resetting static listeners without explanatory comment
@After
public void tearDown() {
  ScreenStateReceiver.resetForTesting();
}
```

Good:

```cpp
// Variant 1: Document why partial check/sentinel update is sufficient
#if PA_CONFIG(THREAD_CACHE_FAST_TLS)
  // This is sufficient to prevent re-creation, because IsTombstone() is called
  // before Create() and IsTombstone() only checks this index.
  PA_UNSAFE_TODO(
      internal::g_thread_caches[internal::kThreadCacheTombstoneIndex]) =
      reinterpret_cast<ThreadCache*>(kTombstone);
#endif

// Variant 2: Explicitly name the cleanup/reset method for unassigned allocation
// Create a FieldTrialList; not a leak because it is cleaned up by the call to
// FieldTrialList::ResetInstance() in FinalizeFeatureState() below.
new base::FieldTrialList();

// Variant 3: Document why apparent optimization is unsafe for untrusted data
// The histogram is from an external source, so `name_hash()` may be invalid.
// Use FindHistogram() without precomputed hash to recompute safely.
HistogramBase* existing = StatisticsRecorder::FindHistogram(lookup_name);

// Variant 4: Document why canonical throwing methods must be avoided
// Note: Do not use bitset::set, reset(size_t), or test; they can throw
// std::out_of_range and pull in runtime symbols causing linker conflicts.
std::bitset<kMaxSuperPagesInPool> alloc_bitset_ PA_GUARDED_BY(lock_);

// Variant 5: Document persistent state, observers, or singletons being cleared
@After
public void tearDown() {
  // Reset to ensure our observers are removed and statics are reset.
  ScreenStateReceiver.resetForTesting();
}
```
