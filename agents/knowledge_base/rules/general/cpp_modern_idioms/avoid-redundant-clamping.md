---
name: general/avoid-redundant-clamping
rubric: readability
severity: nit
---

**Rule.** Do not perform redundant clamping operations or mutate variables to a
default threshold before conditional checks. When bounding values, compute the
clamped result directly in a single initialization expression rather than
repeating `std::min`, `std::max`, or `std::clamp` on already-constrained
variables. When testing whether a value meets or falls below a fallback
threshold, evaluate the boundary condition directly with a relational check
(e.g., `<=`, `>=`) instead of clamping the value to the default in a temporary
variable and checking for equality.

**Why.** Redundant clamping operations and intermediate variable mutations
clutter code, obscure data flow, and introduce unnecessary conditional
branching. Computing bounded values in a single expression or testing thresholds
directly with relational operators clarifies intent and keeps control flow
concise.

## Examples

Bad:

```cpp
// Redundant repeated clamping:
returned /= sizeof(HMODULE);
returned = std::min(kSize, returned);
size_t modules_count = std::min<size_t>(kSize, returned);

// Redundant clamping before equality check:
int depth = initial_nesting_level;
if (depth < kDefaultInitialNestingLevel) {
  depth = kDefaultInitialNestingLevel;
}
if (depth == kDefaultInitialNestingLevel) {
  UseDefaultConfig();
}
```

Good:

```cpp
// Compute clamped value directly in initialization:
const size_t modules_count =
    std::min<size_t>(kSize, returned / sizeof(HMODULE));

// Test boundary condition directly with relational check:
if (initial_nesting_level <= kDefaultInitialNestingLevel) {
  UseDefaultConfig();
}
```
