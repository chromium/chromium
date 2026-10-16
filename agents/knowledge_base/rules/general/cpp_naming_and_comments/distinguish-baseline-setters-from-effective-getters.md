---
name: general/distinguish-baseline-setters-from-effective-getters
rubric: readability
severity: nit
---

**Rule.** Do not use symmetric getter and setter names (e.g., GetFoo() and
SetFoo()) when the setter configures a baseline or default value while the
getter returns a computed or effective value influenced by active leases,
boosts, or overrides. Name the setter explicitly (e.g., SetDefaultFoo()) or
qualify the getter (e.g., GetEffectiveFoo()) to clarify the distinction.

**Why.** Callers expect symmetric GetFoo() and SetFoo() methods to inspect and
modify the exact same internal state. When a getter incorporates dynamic runtime
overrides or active leases while the setter only updates fallback or baseline
state, symmetric naming misleads callers about the side effects and directness
of mutations.

## Examples

Bad:

```cpp
// Misleading: GetCurrentThreadType() returns effective priority with active leases,
// while SetCurrentThreadType() only sets baseline default priority.
static void SetCurrentThreadType(ThreadType thread_type);
static ThreadType GetCurrentThreadType();
```

Good:

```cpp
// Clear distinction: the setter explicitly states it sets the baseline default,
// while the getter returns the current effective value.
static void SetDefaultThreadType(ThreadType thread_type);
static ThreadType GetCurrentThreadType();
```
