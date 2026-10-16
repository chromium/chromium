---
name: general/ensure-sequence-affine-destruction-for-thread-safe-ref-count
rubric: safety
severity: nit
---

**Rule.** Do not invoke `delete this` unconditionally in custom deletion,
release, or destruction hooks of `base::RefCountedThreadSafe` objects that
require sequence-affine teardown. Ensure destruction is dispatched to the bound
`base::SequencedTaskRunner`—by subclassing
`base::RefCountedDeleteOnSequence<T>`, using `base::OnTaskRunnerDeleter`, or
checking `task_runner->RunsTasksInCurrentSequence()` before falling back to
`task_runner->DeleteSoon()` or `ReleaseSoon()`.

**Why.** Thread-safe ref-counted objects can have their final reference dropped
from any thread or thread-pool worker. If destruction runs unconditionally on
the calling thread, sequence-bound members, sequence checkers, and nested
destruction callbacks will execute off-sequence, causing data races,
sequence-checker crashes, and use-after-free bugs.

## Examples

Bad:

```cpp
// In a thread-safe ref-counted class with sequence-affine teardown:
void CustomDeleter::Destruct(const Foo* x) {
  delete x;  // Unsafe: final ref can be dropped on an arbitrary thread.
}

void Foo::DeleteOnCorrectSequence() const {
  delete this;  // Unsafe: drops off-sequence if last ref dropped on worker.
}
```

Good:

```cpp
// Option 1: Inherit from base::RefCountedDeleteOnSequence.
class Foo : public base::RefCountedDeleteOnSequence<Foo> { ... };

// Option 2: Dispatch to the bound sequence explicitly.
void Foo::DeleteOnCorrectSequence() const {
  if (task_runner_->RunsTasksInCurrentSequence()) {
    delete this;
  } else {
    task_runner_->DeleteSoon(FROM_HERE, this);
  }
}
```
