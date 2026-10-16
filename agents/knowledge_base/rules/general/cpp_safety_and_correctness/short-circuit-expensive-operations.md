---
name: general/short-circuit-expensive-operations
rubric: safety
severity: should_fix
---

**Rule.** Do not eagerly compute expensive operations (such as disk I/O, IPC,
complex queries, string operations, or allocations) into intermediate boolean
variables when results are only needed conditionally. Inline operations directly
into compound conditional expressions (||, &&) and order cheaper preconditions
(such as cheap equality checks, boolean flags, or base::FeatureList::IsEnabled)
first so boolean short-circuit evaluation skips unnecessary work.

**Why.** Precomputing expressions into intermediate boolean variables forces
eager evaluation of all checks, bypassing short-circuiting. Placing non-trivial
checks directly inside compound conditions after cheaper predicates ensures
expensive operations (like string formatting, allocations, or disk queries)
execute only when earlier conditions fail to determine the result.

## Examples

Bad:

```cpp
// Variant 1: Eagerly performing string operations and allocations.
bool is_same_bundle = (*parent_bundle_id == *current_bundle_id);
bool is_parent_bundle =
    base::StartsWith(*current_bundle_id, *parent_bundle_id + '.',
                     base::CompareCase::SENSITIVE);
if (is_same_bundle || is_parent_bundle) {
  // ...
}

// Variant 2: Eagerly performing disk I/O before checking feature flags.
bool is_link = base::IsLink(path);
if (recursive && (!is_link || !base::FeatureList::IsEnabled(
                                  features::kPreventReparsePointTraversal))) {
  DeleteFileRecursive(path, pattern, true);
}
```

Good:

```cpp
// Inlining string check after cheap equality check to leverage short-circuiting.
if ((*parent_bundle_id == *current_bundle_id) ||
    base::StartsWith(*current_bundle_id, *parent_bundle_id + '.',
                     base::CompareCase::SENSITIVE)) {
  // ...
}

// Ordering feature flag check first so disk I/O is skipped when disabled.
if (recursive &&
    (!base::FeatureList::IsEnabled(features::kPreventReparsePointTraversal) ||
     !base::IsLink(path))) {
  DeleteFileRecursive(path, pattern, true);
}
```
