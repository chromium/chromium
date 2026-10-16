---
name: general/reuse-member-sentinels-for-moved-state
rubric: safety
severity: should_fix
---

**Rule.** Do not add a dedicated boolean member variable (such as `moved_`,
`is_moved_`, or `disarmed_`) to track whether an object has been moved from or
deactivated if an existing member variable (such as a string, callback, pointer,
or container) has a natural empty or null state that can serve as a sentinel.
Clear or reset that existing member upon move or deactivation, and check its
empty or null state in the destructor.

**Why.** Adding a separate boolean flag increases object layout size due to
alignment padding and introduces redundant state. Reusing an existing member's
empty or null representation unifies the state into a single source of truth and
keeps object memory layout compact.

## Examples

Bad:

```cpp
class ScopedUmaHistogramTimer {
 public:
  ScopedUmaHistogramTimer(ScopedUmaHistogramTimer&& other)
      : name_(std::move(other.name_)), moved_(false) {
    other.moved_ = true;
  }
  ~ScopedUmaHistogramTimer() {
    if (moved_) {
      return;
    }
    // Record histogram...
  }
 private:
  std::string name_;
  bool moved_ = false;
};
```

Good:

```cpp
class ScopedUmaHistogramTimer {
 public:
  ScopedUmaHistogramTimer(ScopedUmaHistogramTimer&& other)
      : name_(std::exchange(other.name_, "")) {}
  ~ScopedUmaHistogramTimer() {
    if (name_.empty()) {
      return;
    }
    // Record histogram...
  }
 private:
  std::string name_;
};
```
