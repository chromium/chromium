---
name: general/omit-nullable-on-local-variables
rubric: readability
severity: nit
---

**Rule.** In `@NullMarked` Java code, do not annotate local variables with
`@Nullable`. Annotate fields, parameters, and return types only.

**Why.** `styleguide/java/nullaway.md`: "Nullness of local variables is inferred
(do not annotate them with `@Nullable`)." The annotation is redundant clutter.

## Examples

Bad:

```java
@Nullable Pair<VirtualDocumentPath, String> pair = splitPath();
if (pair != null) {
    process(pair);
}
```

Good:

```java
Pair<VirtualDocumentPath, String> pair = splitPath();
if (pair != null) {
    process(pair);
}
```
