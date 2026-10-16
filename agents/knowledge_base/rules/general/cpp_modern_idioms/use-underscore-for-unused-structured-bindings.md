---
name: general/use-underscore-for-unused-structured-bindings
rubric: readability
severity: nit
---

**Rule.** When using structured bindings to unpack pairs, tuples, or structs
where some elements are unreferenced (such as container insertion results or
range-based for loops), use an underscore (\_) as a placeholder for the unused
bindings, or access the required member directly. Do not introduce named local
variables for bindings that are not used downstream.

**Why.** Giving named identifiers to unused bindings clutters the local scope,
misleads readers into expecting the variable to be referenced, and can trigger
compiler unused variable warnings. Using the underscore placeholder (\_) or
direct member access clearly documents that the element is intentionally
ignored.

## Examples

Bad:

```cpp
// Bad: 'it' is bound but never used downstream.
auto [it, inserted] = trial_groups.emplace(trial, group);
if (!inserted) {
  return false;
}

// Bad: 'key' is bound but only 'config' is used in the loop.
for (const auto& [key, config] : configs_by_name) {
  ApplyConfig(config);
}
```

Good:

```cpp
// Good: '_' clearly signals that the iterator element is intentionally ignored.
auto [_, inserted] = trial_groups.emplace(trial, group);
if (!inserted) {
  return false;
}

// Good: use '_' for the unreferenced map key.
for (const auto& [_, config] : configs_by_name) {
  ApplyConfig(config);
}
```
