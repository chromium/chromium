---
name: general/remove-unused-test-friend-declarations
rubric: readability
severity: nit
---

**Rule.** Do not retain unused test friend declarations (such as friend class
SomeTest or FRIEND_TEST_ALL_PREFIXES(...)) in production class definitions when
unit tests interact exclusively through public APIs or mocks without accessing
private members.

**Why.** Unused friend declarations are dead code that unnecessarily weakens
encapsulation, clutters class definitions, and couples production interfaces to
specific test fixtures.

## Examples

Bad:

```cpp
class ActiveCollection {
 public:
  void Process();

 private:
  friend class ThreadGroupProfilerTest;
  std::unique_ptr<Profiler> CreateSamplingProfilerForThread(...);
};
```

Good:

```cpp
class ActiveCollection {
 public:
  void Process();

 private:
  std::unique_ptr<Profiler> CreateSamplingProfilerForThread(...);
};
```
