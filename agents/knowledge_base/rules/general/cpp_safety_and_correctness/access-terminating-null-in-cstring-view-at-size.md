---
name: general/access-terminating-null-in-cstring-view-at-size
rubric: correctness
severity: should_fix
---

**Rule.** When accessing the terminating null character of a
`base::basic_cstring_view` (e.g. `base::cstring_view`, `base::wcstring_view`, or
`base::u16cstring_view`) via `operator[]`, index at `view.size()` (or
`view.length()`), never `view.size() + 1`. In a 0-indexed view of length N,
character elements span indices 0 through N - 1, and the terminating null
resides at offset N.

**Why.** `base::basic_cstring_view::operator[]` allows safe indexed access up to
and including `size()` to inspect the terminating null character under an
internal `CHECK_LE(idx, len_)` assertion. Indexing at `size() + 1` is an
off-by-one bounds violation that fails the check and triggers a runtime crash.

## Examples

Bad:

```cpp
base::wcstring_view var = ...;
const size_t length = var.size();
// Off-by-one: index `length + 1` exceeds view bounds and crashes in CHECK_LE.
CHECK_EQ(var[length + 1], L'\0');
```

Good:

```cpp
base::wcstring_view var = ...;
const size_t length = var.size();
// Safe: index `length` (view.size()) accesses the terminating null.
CHECK_EQ(var[length], L'\0');
```
