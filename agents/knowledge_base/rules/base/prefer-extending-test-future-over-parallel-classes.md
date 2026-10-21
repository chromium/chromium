---
name: base/prefer-extending-test-future-over-parallel-classes
rubric: design
severity: should_fix
---

**Rule.** When adding asynchronous callback waiting capabilities in base::test,
prefer extending base::test::TestFuture with configuration modes (such as
TestFutureMode::kQueue) rather than introducing separate parallel future
classes. Ensure extended capabilities retain support for sequence-bound
callbacks, move semantics, and void specializations.

**Why.** Creating separate parallel test future classes duplicates
synchronization, waiting, and sequence-safety logic while frequently omitting
core capabilities like GetSequenceBoundCallback(), move-only value capture, or
void specializations. Consolidating modes into base::test::TestFuture maintains
a single, feature-complete API across Chromium tests.

## Examples

Bad:

```cpp
// Introducing a separate parallel future class that duplicates synchronization:
template <typename... Types>
class QueuedTestFuture {
 public:
  base::RepeatingCallback<void(Types...)> GetRepeatingCallback();
  [[nodiscard]] std::tuple<Types...> Take();
  ...
 private:
  std::deque<std::tuple<Types...>> queue_;
};
```

Good:

```cpp
// Extending TestFuture with a mode enum to support queueing multiple values:
enum class TestFutureMode {
  kSingle,
  kQueue,
};

template <typename... Types>
class TestFuture {
 public:
  explicit TestFuture(TestFutureMode mode = TestFutureMode::kSingle);
  ...
};
```
