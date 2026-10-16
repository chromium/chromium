---
name: general/use-guarded-by-context-with-ts-unchecked-read-for-shutdown
rubric: safety
severity: nit
---

**Rule.** Annotate sequence-affine member variables with
GUARDED_BY_CONTEXT(\<sequence_checker>) even if they are accessed off-sequence
during exceptional lifecycle phases like shutdown or destruction. Bypass
compiler thread safety analysis warnings at the provably safe off-sequence
access site using TS_UNCHECKED_READ() accompanied by an explanatory comment or
assertion proving the access is race-free.

**Why.** Omitting GUARDED_BY_CONTEXT on member variables because of an
exceptional out-of-sequence access during shutdown or destruction forfeits
static thread safety checks across all regular methods. Retaining the annotation
with TS_UNCHECKED_READ() maintains compile-time analysis across the class while
documenting why the exceptional access is safe.

## Examples

Bad:

```cpp
// Header
std::optional<ActiveCollection> active_collection_;

// Implementation
void ThreadGroupProfiler::Shutdown() {
  CHECK(!task_runner_->PostTask(FROM_HERE, DoNothing()));
  active_collection_.reset();
}
```

Good:

```cpp
// Header
std::optional<ActiveCollection> active_collection_
    GUARDED_BY_CONTEXT(task_runner_sequence_checker_);

// Implementation
void ThreadGroupProfiler::Shutdown() {
  CHECK(!task_runner_->PostTask(FROM_HERE, DoNothing()));
  TS_UNCHECKED_READ(active_collection_).reset();
}
```
