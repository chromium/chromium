---
name: general/use-non-joinable-threads-for-fire-and-forget-work
rubric: design
severity: should_fix
---

**Rule.** When launching a low-level physical thread for fire-and-forget
background work that does not need to be synchronized with or waited on by the
caller, create it as non-joinable (e.g., via
`base::PlatformThread::CreateNonJoinableWithType()`, `CreateNonJoinable()`, or
setting `SimpleThread::Options::joinable = false`). Do not create wrapper
structs or retain thread handles across callers merely to invoke `Join()` at
shutdown.

**Why.** When background work produces no result needed by the caller and
requires no shutdown coordination, creating joinable threads forces callers to
plumb lifetime tracking state and keeps the OS thread handle open until `Join()`
runs at process exit. Non-joinable threads let the OS and delegate release
resources immediately upon completion, eliminating wrapper boilerplate and
preventing open handle retention.

## Examples

Bad:

```cpp
struct PreReadSupport {
  std::unique_ptr<base::DelegateSimpleThread> prereading_thread;
  std::unique_ptr<DLLPreReader> dll_prereader;
};

// Inside loader:
preread_support.dll_prereader = std::make_unique<DLLPreReader>(module);
preread_support.prereading_thread =
    std::make_unique<base::DelegateSimpleThread>(
        preread_support.dll_prereader.get(), "ModulePrereadingThread");
preread_support.prereading_thread->StartAsync();

// In main launcher:
int rc = chrome_main(...);
preread_support.prereading_thread->Join();
```

Good:

```cpp
class DLLPreReader : public base::PlatformThread::Delegate {
 public:
  explicit DLLPreReader(const base::FilePath& module) : module_(module) {}
  void ThreadMain() override {
    base::PreReadFile(module_, /*is_executable=*/true, /*sequential=*/false);
    // As a non-joinable thread delegate, clean up when finished.
    delete this;
  }
 private:
  ~DLLPreReader() override = default;
  base::FilePath module_;
};

// Inside loader:
base::PlatformThread::CreateNonJoinableWithType(
    0, new DLLPreReader(module), base::ThreadType::kDefault);
```
