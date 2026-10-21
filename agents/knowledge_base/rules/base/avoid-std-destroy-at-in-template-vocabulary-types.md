---
name: base/avoid-std-destroy-at-in-template-vocabulary-types
rubric: safety
severity: should_fix
---

**Rule.** In template vocabulary types, variant storage, and custom container
implementations (such as base::expected or union storage), destroy stored
elements by calling their destructor directly (e.g., storage\_.value.~T() or
ptr->~T()) rather than using wrapper utilities like
std::destroy_at(std::addressof(...)).

**Why.** In heavily instantiated templates, wrapper functions like
std::destroy_at add extra function call layers and address calculations that can
fail compiler inlining heuristics. When inlining fails across thousands of
translation units, out-of-line wrapper thunks multiply and cause significant
binary size bloat, whereas direct destructor calls inline reliably.

## Examples

Bad:

```cpp
// Bad: Using std::destroy_at wrapper in heavily instantiated template storage.
constexpr void DestroyStorage() {
  switch (state_) {
    case State::kValue:
      std::destroy_at(std::addressof(storage_.value));
      break;
    case State::kError:
      std::destroy_at(std::addressof(storage_.error));
      break;
  }
}
```

Good:

```cpp
// Good: Directly invoking the member destructor avoids inlining failures.
constexpr void DestroyStorage() {
  switch (state_) {
    case State::kValue:
      storage_.value.~T();
      break;
    case State::kError:
      storage_.error.~E();
      break;
  }
}
```
