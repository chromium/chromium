---
name: general/annotate-transient-view-structs-with-stack-allocated-or-own
rubric: readability
severity: should_fix
---

**Rule.** To restrict a class, struct, iterator, or RAII scope to stack
allocation, declare `STACK_ALLOCATED()` from `base/memory/stack_allocated.h` in
the class definition instead of manually deleting `operator new` or
`operator new[]`. Annotate transient parameter bundles and helpers holding
non-owning views (`std::string_view`, `base::span`) or non-owning references
with `STACK_ALLOCATED()`, and prefer storing bounds-safe view types over raw
buffer pointers (`const CharT*` or `T*`). If instances must be heap-allocated or
stored across longer-lived scopes, store owning types (`std::string`,
`std::vector`) instead.

**Why.** `STACK_ALLOCATED()` is the canonical Chromium mechanism to declare
stack-only types. Beyond deleting standard and placement `operator new`, it
enables the `StackAllocatedChecker` Clang plugin at compile time to prevent heap
allocations, disallow embedding as fields of non-stack-allocated classes, and
permit non-owning view types and raw pointers on the stack without `raw_ptr<T>`
overhead or dangling pointer hazards.

## Examples

Bad:

```cpp
// BAD: Manually deleting operator new instead of using STACK_ALLOCATED().
class ScopedServiceBindingBatch {
 public:
  void* operator new(size_t) = delete;
  void* operator new[](size_t) = delete;
};

// BAD: Missing STACK_ALLOCATED() on view struct, or raw pointers in stack iterator.
struct SharedMemorySwitch {
  std::string_view switch_name;
};

class Iterator {
  STACK_ALLOCATED();
 private:
  const wchar_t* block_start_ = nullptr;
};
```

Good:

```cpp
#include "base/memory/stack_allocated.h"

// GOOD: Use STACK_ALLOCATED() for stack-only classes, view structs, and iterators.
class ScopedServiceBindingBatch {
  STACK_ALLOCATED();
 public:
  ...
};

struct SharedMemorySwitch {
  STACK_ALLOCATED();
  std::string_view switch_name;
};

class Iterator {
  STACK_ALLOCATED();
 private:
  NativeEnvironmentStringView block_start_;
};
```
