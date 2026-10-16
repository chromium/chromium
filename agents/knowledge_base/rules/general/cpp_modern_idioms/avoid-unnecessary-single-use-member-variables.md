---
name: general/avoid-unnecessary-single-use-member-variables
rubric: readability
severity: should_fix
---

**Rule.** Do not store objects, task runners, helper instances, or configuration
parameters as class or test fixture data members if they are only needed during
construction, setup, or within a single method. When object initialization or
resource creation is deferred to explicit methods (such as `Create*()` or
`Init()`), pass creation-only options directly to those methods rather than
accepting them in the constructor and retaining them as member state. Construct
single-use helpers inline at the call site or keep them as local variables.

**Why.** Retaining unnecessary member variables adds clutter and state
complexity, can inadvertently extend the lifetime of ref-counted objects, and
misleads readers into expecting the state to be inspected or reused across
member functions. In movable resource wrappers, storing one-off creation
parameters introduces asymmetric move semantics where resource handles are moved
or reset while configuration members linger.

## Examples

Bad:

```cpp
// Bad: Storing task runner or helper needed only during SetUp()
class ThreadGroupProfilerTest : public testing::Test {
 public:
  void SetUp() override {
    profiler_task_runner_ = ThreadPool::CreateSequencedTaskRunner({});
    profiler_ = std::make_unique<ThreadGroupProfiler>(profiler_task_runner_, "Test");
  }
 private:
  scoped_refptr<SequencedTaskRunner> profiler_task_runner_;
  std::unique_ptr<ThreadGroupProfiler> profiler_;
};

// Bad: Storing creation parameter as member state causes asymmetric move semantics
class ScopedTempDir {
 public:
  explicit ScopedTempDir(const FilePath::StringType& prefix);
  ScopedTempDir(ScopedTempDir&& other) noexcept
      : path_(other.Take()), prefix_(other.prefix_) {}
  bool CreateUniqueTempDir();

 private:
  FilePath path_;
  FilePath::StringType prefix_;
};
```

Good:

```cpp
// Good: Construct inline or keep local to SetUp()
class ThreadGroupProfilerTest : public testing::Test {
 public:
  void SetUp() override {
    profiler_ = std::make_unique<ThreadGroupProfiler>(
        ThreadPool::CreateSequencedTaskRunner({}), "Test");
  }
 private:
  std::unique_ptr<ThreadGroupProfiler> profiler_;
};

// Good: Pass creation-only options directly to the creation method
class ScopedTempDir {
 public:
  ScopedTempDir();
  ScopedTempDir(ScopedTempDir&& other) noexcept : path_(other.Take()) {}
  bool CreateUniqueTempDir(FilePath::StringViewType prefix = GetTempDirPrefix());

 private:
  FilePath path_;
};
```
