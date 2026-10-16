---
name: general/prefer-index-zero-for-single-element-invariants
rubric: readability
severity: should_fix
---

**Rule.** When class invariants, assertions, or state machines guarantee that a
collection contains at most a single element (such as an exclusive sentinel
entry or singleton callback) in a given state, access index 0 (.get(0) or [0])
directly rather than querying the last element via size() - 1 (or .back()).

**Why.** Querying the last element via size() - 1 obscures single-element
invariants and misleadingly implies to readers that the collection may contain
multiple entries in that state. Accessing index 0 directly reinforces the
invariant that only a single item or sentinel can exist in that state.

## Examples

Bad:

```cpp
// Anti-pattern: Querying size() - 1 misleadingly implies multiple callbacks can exist
private boolean isThrowingRejectionException() {
    return !mRejectCallbacks.isEmpty()
            && mRejectCallbacks.get(mRejectCallbacks.size() - 1) == null;
}
```

Good:

```cpp
// Preferred: Accessing index 0 communicates and reinforces the single-element invariant
private boolean isThrowingRejectionException() {
    return !mRejectCallbacks.isEmpty() && mRejectCallbacks.get(0) == null;
}
```
