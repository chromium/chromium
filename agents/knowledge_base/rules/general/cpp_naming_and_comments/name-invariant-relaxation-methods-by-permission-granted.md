---
name: general/name-invariant-relaxation-methods-by-permission-granted
rubric: readability
severity: should_fix
---

**Rule.** When naming methods or functions that relax invariants, permit
exceptional conditions, or bypass assertions (such as allowing reentrancy on an
otherwise non-reentrant container), name them after the permission granted or
condition permitted (e.g., using 'Allow...' or '...EvenIf...') rather than using
adverbs (e.g., 'NotifyReentrantly') that misleadingly imply the call itself is
guaranteed or required to be in that state.

**Why.** Adverbs like 'Reentrantly' misleadingly suggest that the invocation
itself is guaranteed or required to occur in that state. Naming the method after
the permission granted (such as 'NotifyAllowReentrancy') accurately communicates
that the method relaxes a restriction and permits the condition without
asserting that it always occurs.

## Examples

Bad:

```cpp
// Misleadingly implies this notification is guaranteed to be reentrant.
void NotifyReentrantly(Method method, const Args&... args);
```

Good:

```cpp
// Accurately describes that reentrancy is permitted during notification.
void NotifyAllowReentrancy(Method method, const Args&... args);
```
