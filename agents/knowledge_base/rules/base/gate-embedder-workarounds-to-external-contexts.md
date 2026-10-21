---
name: base/gate-embedder-workarounds-to-external-contexts
rubric: correctness
severity: should_fix
---

**Rule.** When introducing workarounds, state adjustments, or synchronization
hooks tailored for third-party host run loops or external embedder environments
(such as WebView or attached external message pumps), gate the initialization
and recovery logic strictly to those external execution modes. Do not
unconditionally mutate shared internal state (such as nesting levels, scopes, or
flags) or execute embedder-specific compensatory adjustments on default
execution paths when the opt-in feature is inactive.

**Why.** Unconditionally mutating internal state or running synchronization
logic for opt-in embedder hooks alters behavior and causes regressions along
standard execution paths. Restricting initialization and compensatory
workarounds strictly to active external attachment modes preserves standard
runtime guarantees and invariants.

## Examples

Bad:

```cpp
// Bad: Modifying state or running compensatory logic unconditionally
void MessagePumpCFRunLoopBase::OnAttach() {
  CHECK_EQ(nesting_level_, 0);
  for (int i = 0; i < g_initial_nesting_level; ++i) {
    ++nesting_level_;
    PushWorkItemScope();
  }
}

void MessagePumpCFRunLoopBase::PopWorkItemScope() {
  --nesting_level_;
  if (run_nesting_level_ > nesting_level_) {
    run_nesting_level_ = nesting_level_;
  }
}
```

Good:

```cpp
// Good: Gating initialization and recovery behind the active external mode
void MessagePumpCFRunLoopBase::OnAttach() {
  CHECK_EQ(nesting_level_, 0);
  if (!is_using_host_nesting_sync) {
    nesting_level_ = 1;
    PushWorkItemScope();
    return;
  }
  for (int i = 0; i < initial_nesting_level; ++i) {
    ++nesting_level_;
    PushWorkItemScope();
  }
}

void MessagePumpCFRunLoopBase::PopWorkItemScope() {
  --nesting_level_;
  if (is_attached_to_external_loop_ && run_nesting_level_ > nesting_level_) {
    run_nesting_level_ = nesting_level_;
    ScheduleWork();
  }
}
```
