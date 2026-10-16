---
name: general/do-not-post-to-inherited-thread-type-runner-from-higher-prio
rubric: correctness
severity: should_fix
---

**Rule.** Do not post tasks to a SequencedTaskRunner or SingleThreadTaskRunner
configured with base::InheritThreadType() from a thread or task context of
higher priority than the context in which the task runner was created. While a
higher-priority context can share its task runner with lower-priority callers,
posting from a higher-priority context triggers a DCHECK because sequenced
runners freeze the thread type at creation time.

**Why.** Sequenced and single-thread task runners in base::ThreadPool fix the
sequence's ThreadType once at creation time using GetCurrentTaskImportance().
Posting a task to an existing runner cannot elevate its thread priority, and
ThreadPoolImpl enforces via DCHECK that posting contexts do not exceed the
runner's created priority.

## Examples

Bad:

```cpp
// Created in a background or low-priority task context:
auto runner = base::ThreadPool::CreateSequencedTaskRunner({base::InheritThreadType()});
// ... later posted to from a higher-priority context (e.g. USER_BLOCKING):
runner->PostTask(FROM_HERE, base::BindOnce(&DoWork)); // Triggers DCHECK!
```

Good:

```cpp
// Created in a high-priority context and optionally shared with lower-priority callers:
auto runner = base::ThreadPool::CreateSequencedTaskRunner({base::InheritThreadType()});
// ... posted to from the same or lower-priority context:
runner->PostTask(FROM_HERE, base::BindOnce(&DoWork));
```
