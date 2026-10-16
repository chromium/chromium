---
name: general/use-std-exchange-to-extract-and-reset-containers
rubric: readability
severity: nit
---

**Rule.** When transferring and resetting the contents of a container,
aggregate, or value type (such as `std::array`), use
`std::exchange(container, {})` or direct assignment instead of writing manual
element-by-element copy-and-clear loops.

**Why.** Standard containers and aggregates support value semantics with
built-in copy, move, and assignment operations. Using
`std::exchange(container, {})` expresses the transfer and reset atomically in a
single concise, idiomatic statement, eliminating multi-line loops and avoiding
index errors.

## Examples

Bad:

```cpp
for (size_t i = 0; i < shared_worker_threads_.size(); ++i) {
  for (size_t j = 0; j < shared_worker_threads_[i].size(); ++j) {
    local_shared_worker_threads[i][j] = shared_worker_threads_[i][j];
    shared_worker_threads_[i][j] = nullptr;
  }
}
```

Good:

```cpp
local_shared_worker_threads = std::exchange(shared_worker_threads_, {});
```
