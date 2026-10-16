---
name: general/separate-public-initiation-from-retry-execution
rubric: design
severity: should_fix
---

**Rule.** When designing asynchronous retry routines or multi-attempt
operations, separate the public initiation method (e.g., Start(...)) from the
internal attempt execution step (e.g., RunTask() or DoAttempt()). Do not
re-invoke public initiation methods across retries or pass dummy/empty callbacks
to satisfy initiation parameters.

**Why.** Re-invoking public initiation methods on retries forces passing dummy
callbacks, complicates control flow with retry-state branching, and prevents
asserting that the operation is not already running. Separating initiation from
execution allows the public method to validate entry preconditions and store
callbacks cleanly while keeping attempt execution self-contained.

## Examples

Bad:

```cpp
void RetryStrategy::Start(base::OnceCallback<void(bool)> done_callback) {
  if (done_callback_) {
    // Retry path: callback already stored.
    DCHECK(done_callback.is_null());
  } else {
    done_callback_ = std::move(done_callback);
  }
  task_->Run(base::BindOnce(&RetryStrategy::OnTaskFinished, GetWeakPtr()));
}

void RetryStrategy::OnTaskFinished(bool success) {
  if (!success && retry_count_++ < max_retries_) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&RetryStrategy::Start, GetWeakPtr(), base::NullCallback()),
        delay_);
    return;
  }
  std::move(done_callback_).Run(success);
}
```

Good:

```cpp
void RetryStrategy::Start(base::OnceCallback<void(bool)> done_callback) {
  DCHECK(done_callback_.is_null());
  done_callback_ = std::move(done_callback);
  RunTask();
}

void RetryStrategy::RunTask() {
  task_->Run(base::BindOnce(&RetryStrategy::OnTaskFinished, GetWeakPtr()));
}

void RetryStrategy::OnTaskFinished(bool success) {
  if (!success && retry_count_++ < max_retries_) {
    task_runner_->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&RetryStrategy::RunTask, GetWeakPtr()),
        delay_);
    return;
  }
  std::move(done_callback_).Run(success);
}
```
