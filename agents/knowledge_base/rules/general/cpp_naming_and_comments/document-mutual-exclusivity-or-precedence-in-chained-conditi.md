---
name: general/document-mutual-exclusivity-or-precedence-in-chained-conditi
rubric: readability
severity: nit
---

**Rule.** When chained conditional branches (such as `if ... else if ...` or
sequential early-exit checks) evaluate runtime state predicates or conditions
that could appear capable of holding simultaneously, add an inline comment
explaining whether the conditions are mutually exclusive or documenting the
rationale for branch precedence order.

**Why.** Sequential conditional branches imply precedence when conditions could
potentially occur simultaneously. Explaining that conditions are strictly
mutually exclusive (or why one takes precedence over another) clarifies
non-obvious runtime invariants and prevents invalid assumptions during future
refactorings.

## Examples

Bad:

```cpp
MessagePumpType GetCurrentMessagePumpType() {
  if (CurrentIOThread::IsSet()) {
    return MessagePumpType::IO;
  } else if (CurrentUIThread::IsSet()) {
    return MessagePumpType::UI;
  }
  return MessagePumpType::DEFAULT;
}
```

Good:

```cpp
MessagePumpType GetCurrentMessagePumpType() {
  // CurrentIOThread::IsSet() and CurrentUIThread::IsSet() can't both be set at
  // the same time, so there's no precedence to worry about: both
  // CurrentIOThread::IsSet() and CurrentUIThread::IsSet() rely on
  // GetCurrentSequenceManagerImpl(), which returns the single SequenceManager
  // instance bound to the current thread.
  if (CurrentIOThread::IsSet()) {
    return MessagePumpType::IO;
  } else if (CurrentUIThread::IsSet()) {
    return MessagePumpType::UI;
  }
  return MessagePumpType::DEFAULT;
}
```
