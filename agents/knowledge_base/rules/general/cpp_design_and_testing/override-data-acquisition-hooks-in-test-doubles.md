---
name: general/override-data-acquisition-hooks-in-test-doubles
rubric: design
severity: should_fix
---

**Rule.** Do not fragment simple synchronous routines or timer callbacks into
redundant intermediate scheduling and completion methods (e.g., ScheduleFoo()
immediately delegating to OnFooComplete(ComputeFoo())). Keep synchronous
dispatch and orchestration unified in a single handler (such as OnFooTimer()),
make the underlying data-acquisition method a protected virtual hook to override
in test doubles, and expose the protected timer callback (e.g., via using
BaseClass::OnFooTimer;) for test driving.

**Why.** Splitting synchronous routines into separate scheduling and completion
steps adds unnecessary indirection and boilerplate without providing
asynchronous execution benefits. In test doubles, overriding dispatch methods
bypasses essential orchestration logic such as sequence checks, caching, and
observer notifications, whereas virtual data-acquisition hooks allow fakes to
supply mock data while fully exercising production dispatch paths.

## Examples

Bad:

```cpp
// Bad: Redundantly splitting synchronous routines into Schedule/Complete steps,
// and overriding the orchestration dispatcher in test fakes.
void MemoryMonitor::ScheduleMemoryCheck() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  OnMemoryCheckComplete(ComputeAvailableMemory());
}

class FakeMemoryMonitor : public MemoryMonitor {
 public:
  void ScheduleMemoryCheck() override {
    OnMemoryCheckComplete(fake_bytes_);
  }
 private:
  base::ByteSize fake_bytes_;
};
```

Good:

```cpp
// Good: Keep synchronous handling unified in one handler and override virtual data hook in tests.
void MemoryMonitor::OnMemoryCheckTimer() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::optional<base::ByteSize> memory = ComputeAvailableMemory();
  if (!memory.has_value()) {
    last_sample_.reset();
    return;
  }
  NotifyObservers(*memory);
}

class TestMemoryMonitor : public MemoryMonitor {
 public:
  using MemoryMonitor::OnMemoryCheckTimer;

 protected:
  std::optional<base::ByteSize> ComputeAvailableMemory() override {
    return fake_bytes_;
  }
};
```
