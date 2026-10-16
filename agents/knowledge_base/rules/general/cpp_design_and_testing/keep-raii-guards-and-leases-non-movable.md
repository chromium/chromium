---
name: general/keep-raii-guards-and-leases-non-movable
rubric: design
severity: should_fix
---

**Rule.** Do not provide move constructors or move assignment operators for RAII
guard, lease, or scope-management classes unless there is an explicit
requirement to transfer ownership across scopes. Keep such classes non-copyable
and non-movable (by explicitly deleting copy operations) so member variables can
remain direct values rather than being wrapped in std::optional or nullable
representations.

**Why.** Supporting move semantics requires the class to represent and handle a
disarmed or moved-from state. This complicates class invariants, introduces
unnecessary std::optional wrappers or sentinel checks to member variables and
accessors, and increases lifecycle complexity.

## Examples

Bad:

```cpp
class RaiseThreadTypeLease {
 public:
  explicit RaiseThreadTypeLease(ThreadType thread_type);
  RaiseThreadTypeLease(RaiseThreadTypeLease&&);
  RaiseThreadTypeLease& operator=(RaiseThreadTypeLease&&);

  std::optional<ThreadType> thread_type() const {
    return leased_thread_type_;
  }
 private:
  std::optional<ThreadType> leased_thread_type_;
};
```

Good:

```cpp
class RaiseThreadTypeLease {
 public:
  explicit RaiseThreadTypeLease(ThreadType thread_type);
  RaiseThreadTypeLease(const RaiseThreadTypeLease&) = delete;
  RaiseThreadTypeLease& operator=(const RaiseThreadTypeLease&) = delete;

  ThreadType thread_type() const { return leased_thread_type_; }
 private:
  ThreadType leased_thread_type_;
};
```
