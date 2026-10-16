---
name: general/bind-reference-via-ternary-to-avoid-branch-duplication
rubric: readability
severity: nit
---

**Rule.** When performing an identical operation (such as container insertion,
method invocation, or assignment) on one of two target containers or objects
based on a boolean condition, bind a reference to the selected target using a
ternary conditional operator (`auto& target = cond ? a : b;`) and perform the
operation once. Avoid duplicating the operation across `if` and `else` branches.

**Why.** Duplicating identical statements across if/else branches when only the
receiver or target container differs introduces visual clutter and maintenance
overhead. Decoupling target selection from the subsequent operation ensures that
mutating or accessing logic is defined and maintained in a single place.

## Examples

Bad:

```cpp
// Bad: Duplicating container insertion logic across if/else branches.
if (enable) {
  enabled_features_.emplace_back(std::move(feature_name));
} else {
  disabled_features_.emplace_back(std::move(feature_name));
}
```

Good:

```cpp
// Good: Select target container via ternary reference binding and perform insertion once.
auto& features = enable ? enabled_features_ : disabled_features_;
features.emplace_back(std::move(feature_name));
```
