---
name: base/avoid-throwing-std-bitset-methods-in-partitionalloc
rubric: correctness
severity: should_fix
---

**Rule.** In PartitionAlloc, do not use std::bitset member functions that throw
exceptions on out-of-bounds access (such as test(), set(), reset(size_t), or
flip(size_t)); use operator[] (e.g., bitset[i] = true/false) instead, and add an
explanatory comment where the bitset is declared documenting the restriction.

**Why.** In libc++, std::bitset member functions like test(), set(), and
reset(size_t) invoke std::\_\_throw_out_of_range upon bounds check failure. When
PartitionAlloc is statically linked into standalone binaries or tests with
different allocator/runtime configurations (such as breakpad_unittests in macOS
official builds), this generates unstripped \_\_throw_out_of_range symbols that
cause link-time duplicate symbol errors. Using operator[] relies on assertion
checks instead of exception-throwing stubs, avoiding unwanted symbol exports.

## Examples

Bad:

```cpp
// In PartitionAlloc address_pool_manager.cc:
if (alloc_bitset_.test(curr_bit)) {
  return false;
}
alloc_bitset_.set(i);
alloc_bitset_.reset(i);
```

Good:

```cpp
// In PartitionAlloc address_pool_manager.h:
// Note: Do not use bitset::set(), reset(size_t), or test(), as they call
// std::__throw_out_of_range and cause symbol collisions during linking.
std::bitset<kMaxSuperPagesInPool> alloc_bitset_ PA_GUARDED_BY(lock_);

// In address_pool_manager.cc:
if (alloc_bitset_[curr_bit]) {
  return false;
}
alloc_bitset_[i] = true;
alloc_bitset_[i] = false;
```
