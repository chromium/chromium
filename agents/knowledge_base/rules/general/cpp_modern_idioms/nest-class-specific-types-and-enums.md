---
name: general/nest-class-specific-types-and-enums
rubric: readability
severity: should_fix
---

**Rule.** Nest helper types, configuration structs, and enums inside the
non-templated class they configure (omitting redundant class-name prefixes). For
class templates, define enums and helper types that do not depend on template
parameters outside the class template (in the enclosing namespace or a
non-templated base class) rather than nesting them.

**Why.** Nesting types within non-templated classes keeps the enclosing
namespace clean and avoids redundant prefixes. For class templates, nesting
template-independent enums creates separate, incompatible enum types for every
template instantiation and forces callers to specify template arguments at call
sites.

## Examples

Bad:

```cpp
// Redundant class prefix for non-templated class helper enum:
enum class WidgetState { kIdle, kRunning };
class Widget {
 public:
  explicit Widget(WidgetState state);
};

// Nested template-independent enum inside a class template:
template <typename... Types>
class TestFuture {
 public:
  enum class Mode { kSingle, kQueue };
  explicit TestFuture(Mode mode = Mode::kSingle);
};
// Call site requires template arguments: TestFuture<int>::Mode::kQueue
```

Good:

```cpp
// Nested in non-templated class with concise, unprefixed name:
class Widget {
 public:
  enum class State { kIdle, kRunning };
  explicit Widget(State state);
};

// Template-independent enum defined in enclosing namespace:
enum class TestFutureMode { kSingle, kQueue };

template <typename... Types>
class TestFuture {
 public:
  explicit TestFuture(TestFutureMode mode = TestFutureMode::kSingle);
};
// Clean call site: TestFuture<int> future(TestFutureMode::kQueue);
```
