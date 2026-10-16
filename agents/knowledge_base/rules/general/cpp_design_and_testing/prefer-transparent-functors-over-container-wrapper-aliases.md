---
name: general/prefer-transparent-functors-over-container-wrapper-aliases
rubric: design
severity: should_fix
---

**Rule.** When enabling heterogeneous lookup in associative hash containers
(such as absl::flat_hash_set or absl::flat_hash_map), configure the container
directly using transparent hash and equality functors (such as
base::TransparentHashAs<View> and base::TransparentEqualAs<View>). Do not define
wrapper container template aliases taking key and view type parameters.

**Why.** Multi-argument container wrapper aliases (such as a set alias
parameterized over both key and view types) obscure container semantics and
misleadingly resemble key-value maps. Directly passing transparent policy
functors to standard containers aligns with standard library conventions and
eliminates custom wrapper boilerplate.

## Examples

Bad:

```cpp
// Avoid creating container wrapper aliases that take a key and view type:
template <typename T, typename TransparentT>
using transparent_flat_hash_set =
    absl::flat_hash_set<T,
                        typename internal::TransparentHashEq<T, TransparentT>::Hash,
                        typename internal::TransparentHashEq<T, TransparentT>::Eq>;

transparent_flat_hash_set<std::vector<int>, base::span<const int>> my_set;
```

Good:

```cpp
// Prefer standard containers configured directly with transparent functors:
absl::flat_hash_set<std::vector<int>,
                    base::TransparentHashAs<base::span<const int>>,
                    base::TransparentEqualAs<base::span<const int>>> my_set;
```
