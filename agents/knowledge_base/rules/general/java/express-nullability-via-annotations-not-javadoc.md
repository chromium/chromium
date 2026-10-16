---
name: general/express-nullability-via-annotations-not-javadoc
rubric: readability
severity: should_fix
---

**Rule.** Do not describe nullability constraints in Javadoc comments (e.g.,
'must not be null', 'cannot be null', or 'may be null') for Java method
parameters, return types, or fields. Specify nullability directly in method and
type signatures using nullness annotations (such as @Nullable or @NonNull) or
rely on implicit non-null defaults in @NullMarked classes.

**Why.** Nullness annotations are machine-checkable and enforced at compile time
by static analysis tools (like NullAway), whereas Javadoc prose comments are
unverified, prone to becoming stale, and add redundant documentation noise.

## Examples

Bad:

```cpp
/**
 * @param view The view associated with the selection. This value cannot be null.
 * @param callback The completion callback. May be null.
 * @return The active token, or null if unselected.
 */
public Token setSelection(View view, Callback callback) {
    ...
}
```

Good:

```cpp
/**
 * @param view The view associated with the selection.
 * @param callback The completion callback.
 * @return The active token, or null if unselected.
 */
public @Nullable Token setSelection(View view, @Nullable Callback callback) {
    ...
}
```
