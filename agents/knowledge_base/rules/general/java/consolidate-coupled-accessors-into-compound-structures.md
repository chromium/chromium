---
name: general/consolidate-coupled-accessors-into-compound-structures
rubric: design
severity: optional
---

**Rule.** Avoid proliferating multiple separate accessor methods to extract
correlated fields (such as start/end offsets, coordinates, or related
parameters) from the same data source or Bundle. Consolidate them into a single
method returning a compound structure (such as Pair or a dedicated container
class).

**Why.** Splitting tightly coupled fields into separate accessors bloats
interface surface area, causes redundant parsing and lookups of the underlying
data source, and forces callers to perform repetitive null-checks across
multiple method calls. Returning a single composite structure enables atomic
retrieval and unified presence checks.

## Examples

Bad:

```cpp
// Proliferating separate getters duplicates bundle lookups and bloats the interface.
default @Nullable Integer getSelectionStartVirtualDescendantId(Bundle arguments) {
    return null;
}
default @Nullable Integer getSelectionStartOffset(Bundle arguments) {
    return null;
}
default @Nullable Integer getSelectionEndVirtualDescendantId(Bundle arguments) {
    return null;
}
default @Nullable Integer getSelectionEndOffset(Bundle arguments) {
    return null;
}
```

Good:

```cpp
// Consolidated accessor returning a compound structure for single-call retrieval.
default @Nullable Pair<Pair<Integer, Integer>, Pair<Integer, Integer>> getSelectionArguments(
        Bundle arguments) {
    return null;
}
```
