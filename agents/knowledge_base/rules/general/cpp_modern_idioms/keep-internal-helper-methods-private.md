---
name: general/keep-internal-helper-methods-private
rubric: readability
severity: should_fix
---

**Rule.** Keep internal helper methods, implementation routines, and
state-manipulation utilities (such as internal swap, reset, or calculation
helpers) in the private: section of the class. Do not expose
implementation-detail methods in the public: or protected: sections unless they
are explicitly intended to be part of the class's supported external interface.

**Why.** Exposing internal helper methods in the public interface unnecessarily
widens the class's public API surface, leaks implementation details to callers,
and creates maintenance overhead by making refactoring harder when external
callers begin depending on internal mechanics.

## Examples

Bad:

```cpp
class AutoReset {
 public:
  AutoReset& operator=(AutoReset&& rhs) {
    AutoReset(std::move(rhs)).swap(*this);
    return *this;
  }

  // Internal helper unintentionally exposed in public API:
  void swap(AutoReset& other) {
    std::swap(scoped_variable_, other.scoped_variable_);
    std::swap(original_value_, other.original_value_);
  }
};
```

Good:

```cpp
class AutoReset {
 public:
  AutoReset& operator=(AutoReset&& rhs) {
    AutoReset(std::move(rhs)).swap(*this);
    return *this;
  }

 private:
  // Kept private to protect class encapsulation and API surface:
  void swap(AutoReset& other) {
    std::swap(scoped_variable_, other.scoped_variable_);
    std::swap(original_value_, other.original_value_);
  }
};
```
