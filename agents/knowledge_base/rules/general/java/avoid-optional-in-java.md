---
name: general/avoid-optional-in-java
rubric: readability
severity: nit
---

**Rule.** Do not use `java.util.Optional` (or `OptionalInt`, `OptionalLong`,
etc.) for fields, parameters, or return values in Chromium Java. Use a
`@Nullable` reference (or `@Nullable` boxed primitive), a sentinel value, or an
overload instead. `Optional` is only justified when "null" and "unset" must be
distinguished.

**Why.** `styleguide/java/java.md` says to avoid `Optional`: it adds binary size
and an allocation per use, while NullAway already guarantees that `@Nullable`
values are null-checked.

## Examples

Bad:

```java
default Optional<Integer> getSelectionStartOffset(AccessibilityNodeInfoCompat info) {
    return Optional.empty();
}
```

Good:

```java
default @Nullable Integer getSelectionStartOffset(AccessibilityNodeInfoCompat info) {
    return null;
}
```
