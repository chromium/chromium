---
name: general/prefer-weakptr-over-unretained-for-posted-tasks
rubric: safety
severity: should_fix
---

**Rule.** Do not use `base::Unretained(this)` or raw pointers when binding
callbacks for asynchronous, cross-sequence, or delayed tasks (such as via
`PostTask` or `PostDelayedTask`) unless object lifetime is proven and documented
in a comment. Use a `base::WeakPtr` (e.g., `weak_factory_.GetWeakPtr()`) or
`base::SafeRef` to ensure the callback safely drops if the object is destroyed
before execution.

**Why.** Asynchronous and delayed tasks frequently execute after the target
object has been destroyed, resulting in use-after-free (UAF) vulnerabilities and
dangling pointer crashes. Binding through `base::WeakPtr` automatically
invalidates or drops the callback invocation if the target instance is deleted
before the task runs.

## Examples

Bad:

```cpp
SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
    FROM_HERE,
    base::BindOnce(&RetryStrategy::Start, base::Unretained(this), task_,
                   base::OnceCallback<void(bool)>()),
    delay_);
```

Good:

```cpp
base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
    FROM_HERE,
    base::BindOnce(&RetryStrategy::RunTask, weak_factory_.GetWeakPtr()),
    delay_);
```
