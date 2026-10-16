---
name: general/use-kmax-only-for-inclusive-upper-bounds
rubric: readability
severity: should_fix
---

**Rule.** Use `kMax...` (or `kMaxValue`) only to denote the inclusive upper
bound (the maximum valid value or index) of a range or enum. When a constant
represents the total number of elements, capacity, or an exclusive upper bound,
name it `kNum...` or `k...Count` instead.

**Why.** In Chromium conventions (such as histogram `kMaxValue` and protobuf
`_MAX`), `Max` designates the highest valid inclusive value. Naming an exclusive
limit or element count `kMax...` creates ambiguity and invites off-by-one errors
in loop conditions (`<` vs `<=`), bounds assertions, and array sizing.

## Examples

Bad:

```cpp
// 2 valid tokens (0 and 1), but named kMax instead of kNum:
inline static constexpr AllocToken kMaxAllocToken = AllocToken(2);

PA_DCHECK(alloc_token < kMaxAllocToken);
for (size_t token = 0; token < kMaxAllocToken.value(); ++token) {
  DoWork(token);
}
```

Good:

```cpp
// kMax denotes the highest valid inclusive value:
inline static constexpr AllocToken kMaxAllocToken = AllocToken(1);

PA_DCHECK(alloc_token <= kMaxAllocToken);
for (size_t token = 0; token <= kMaxAllocToken.value(); ++token) {
  DoWork(token);
}
LeakySingleton<...> g_roots[kMaxAllocToken.value() + 1] = {};
```
