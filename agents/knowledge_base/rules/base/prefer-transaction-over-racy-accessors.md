---
name: base/prefer-transaction-over-racy-accessors
rubric: readability
severity: should_fix
---

**Rule.** When holding a `TaskSource::Transaction` or `Sequence::Transaction` in
`base/task/thread_pool`, access properties (such as `thread_type()`, `traits()`)
through the transaction object rather than calling lockless accessors like
`thread_type_racy()` on the underlying sequence or task source. Reserve `*_racy`
methods strictly for contexts where synchronization is not held.

**Why.** Methods with a `_racy` suffix in `base/task/thread_pool` indicate
unsynchronized, lockless reads. Calling `*_racy` methods while a `Transaction`
is held is misleading to readers because it implies an unsynchronized read when
synchronization is already acquired.

## Examples

Bad:

```cpp
auto transaction = sequence->BeginTransaction();
if (!task_tracker_->WillPostTaskNow(task, sequence->thread_type_racy())) {
  return false;
}
```

Good:

```cpp
auto transaction = sequence->BeginTransaction();
if (!task_tracker_->WillPostTaskNow(task, transaction.thread_type())) {
  return false;
}
```
