---
name: context-hub/sqlite-async-dispatch-and-sequence-check
rubric: safety
severity: blocking
---

**Rule.** Dispatch all `ContextHubDatabase` calls on `db_task_runner_` via
`db_.AsyncCall(...)` (queuing in `queued_operations_` while initializing), and
assert `DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_)` at the entry of
every `ContextHubDatabase` method.

**Why.** Running synchronous SQLite queries on the UI thread stalls browser
responsiveness, calling `db_.AsyncCall(...)` before initialization completes
drops or misroutes requests, and accessing `sql::Database` without sequence
verification violates `GUARDED_BY_CONTEXT(sequence_checker_)`.

## Examples

Bad:

```cpp
void ContextHubBackendImpl::DeleteMemoryBankEntries(
    base::span<const int64_t> ids,
    OperationCompleteCallback callback) {
  db_.AsyncCall(&ContextHubDatabase::DeleteMemoryBankEntries)
      .WithArgs(base::ToVector(ids))
      .Then(std::move(callback));
}

bool ContextHubDatabase::DeleteMemoryBankEntries(
    base::span<const int64_t> ids) {
  if (!db_ || !db_->is_open()) {
    return false;
  }
  return memory_bank_table_.DeleteEntries(ids);
}
```

Good:

```cpp
void ContextHubBackendImpl::DeleteMemoryBankEntries(
    base::span<const int64_t> ids,
    OperationCompleteCallback callback) {
  switch (db_state_) {
    case DbState::kUninitialized:
    case DbState::kInitializing:
      queued_operations_.push_back(
          base::BindOnce(&ContextHubBackendImpl::DeleteMemoryBankEntries,
                         weak_ptr_factory_.GetWeakPtr(), base::ToVector(ids),
                         std::move(callback)));
      break;
    case DbState::kReady:
      db_.AsyncCall(&ContextHubDatabase::DeleteMemoryBankEntries)
          .WithArgs(base::ToVector(ids))
          .Then(std::move(callback));
      break;
    case DbState::kFailed:
      std::move(callback).Run(false);
      break;
  }
}

bool ContextHubDatabase::DeleteMemoryBankEntries(
    base::span<const int64_t> ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_ || !db_->is_open()) {
    return false;
  }
  return memory_bank_table_.DeleteEntries(ids);
}
```
