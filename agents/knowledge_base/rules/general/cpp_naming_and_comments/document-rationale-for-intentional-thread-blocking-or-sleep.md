---
name: general/document-rationale-for-intentional-thread-blocking-or-sleep
rubric: readability
severity: should_fix
---

**Rule.** When introducing synchronous thread sleeping
(`base::PlatformThread::Sleep`, `::Sleep`) or blocking operations in code that
may execute on UI threads or task sequences (such as emergency fallback or
last-ditch recovery paths), add an explanatory comment. Document why synchronous
blocking is necessary, why asynchronous alternatives cannot be used, and why
blocking is acceptable in that context.

**Why.** Synchronous blocking and sleeps halt thread execution and freeze event
loops, violating Chromium threading invariants on UI and sequenced task runners.
Documenting the rationale clarifies why blocking is deliberate and acceptable in
that specific emergency or fallback context, preventing reviewers and future
maintainers from mistaking intentional blocking for an anti-pattern.

## Examples

Bad:

```cpp
// Bad: Synchronously sleeping or blocking without documenting why blocking is acceptable.
base::PlatformThread::Sleep(base::Milliseconds(kRecoveryDelayMs));
```

Good:

```cpp
// Good: Clear comment explaining the emergency context and why synchronous blocking is safe.
// A helper process is terminated to reclaim memory. This intentionally blocks the thread
// to allow the OS to liberate pages, but this emergency path is only entered when an
// imminent Out-Of-Memory crash is otherwise inevitable.
base::PlatformThread::Sleep(base::Milliseconds(kRecoveryDelayMs));
```
