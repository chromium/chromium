---
name: general/prefer-if-over-ternary-for-side-effects
rubric: readability
severity: nit
---

**Rule.** Do not use the ternary conditional operator (`? :`) for expressions
that perform side effects, mutate out-parameters, or alter system state. Reserve
ternary operators for pure value selection, and prefer procedural `if`
statements or early returns for state-modifying control flow.

**Why.** Ternary operators are designed for selecting between pure expressions
or values. Using them for mutating operations obscures state changes, reduces
code readability, and makes procedural control flow harder to trace than
standard if/else statements.

## Examples

Bad:

```cpp
// Bad: Ternary operator hides mutating out-param and side-effecting directory creation.
FilePath parent_dir;
return GetSecureTempDirectory(&parent_dir)
           ? CreateTemporaryDirInDir(
                 parent_dir, prefix.empty() ? kDefaultTempDirPrefix : prefix,
                 new_temp_path)
           : false;
```

Good:

```cpp
// Good: Procedural if statements clearly separate validation and side effects.
FilePath parent_dir;
if (!GetSecureTempDirectory(&parent_dir)) {
  return false;
}
return CreateTemporaryDirInDir(
    parent_dir, prefix.empty() ? kDefaultTempDirPrefix : prefix,
    new_temp_path);
```
