---
name: general/no-test-only-stream-operators-in-production
rubric: design
severity: should_fix
---

**Rule.** Do not define test-only streaming operators
(operator\<<(std::ostream&, ...)) in production headers or source files. Move
test-only stream printing overloads to test files (e.g., \*\_unittest.cc) or
testonly targets, and expose a dedicated string conversion helper (e.g.,
returning std::string_view) in production code if stringification is needed.

**Why.** Defining test-only stream operators in production headers bloats binary
size, exports unnecessary symbols, and forces heavy stream header dependencies
onto production code.

## Examples

Bad:

```cpp
// base/threading/platform_thread.h
BASE_EXPORT std::ostream& operator<<(std::ostream& os, ThreadType type);

// base/threading/platform_thread.cc
std::ostream& operator<<(std::ostream& os, ThreadType type) {
  switch (type) {
    case ThreadType::kBackground:
      return os << "Background";
    // ...
  }
}
```

Good:

```cpp
// base/threading/platform_thread.h
BASE_EXPORT std::string_view ThreadTypeToString(ThreadType type);

// base/threading/platform_thread.cc
std::string_view ThreadTypeToString(ThreadType type) {
  switch (type) {
    case ThreadType::kBackground:
      return "Background";
    // ...
  }
}

// base/threading/platform_thread_unittest.cc
std::ostream& operator<<(std::ostream& os, ThreadType type) {
  return os << ThreadTypeToString(type);
}
```
