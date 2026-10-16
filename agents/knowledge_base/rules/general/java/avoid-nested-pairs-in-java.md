---
name: general/avoid-nested-pairs-in-java
rubric: readability
severity: optional
---

**Rule.** Avoid using nested Pair types (e.g., Pair\<Pair\<T, U>, Pair\<V, W>>
or Pair\<T, Pair\<U, V>>) in Java method signatures, return types, or data
structures. Prefer splitting the operation into separate focused methods
returning simpler values or defining a dedicated class with named fields.

**Why.** Nested pairs obscure the semantic meaning of their components and force
callers to use cryptic accessor chains like .first.first and .second.second.
Splitting into focused methods or declaring a dedicated class with named fields
makes APIs self-documenting and prevents ordering bugs.

## Examples

Bad:

```cpp
// Bad: Nested Pair obscures component semantics and requires .first.first / .second.second.
default @Nullable Pair<Pair<Integer, Integer>, Pair<Integer, Integer>>
        getActionSetExtendedSelectionArgument(Bundle arguments) {
    return null;
}
```

Good:

```cpp
// Good: Split into focused methods with clear, self-documenting return types (or use a dedicated class).
default @Nullable Pair<Integer, Integer> getActionSetExtendedSelectionStartArgument(
        Bundle arguments) {
    return null;
}

default @Nullable Pair<Integer, Integer> getActionSetExtendedSelectionEndArgument(
        Bundle arguments) {
    return null;
}
```
