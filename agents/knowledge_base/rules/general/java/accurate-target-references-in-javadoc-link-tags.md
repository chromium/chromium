---
name: general/accurate-target-references-in-javadoc-link-tags
rubric: readability
severity: nit
---

**Rule.** In Javadoc comments, ensure `{@link}` and `{@linkplain}` tags
accurately reference the intended class, compatibility wrapper (such as
`AccessibilityNodeInfoCompat` rather than `AccessibilityNodeInfo`), or member
method signature. Avoid referencing raw framework classes when calling or
delegating through compatibility wrappers, and do not reference non-existent or
mismatched method signatures.

**Why.** Referencing raw framework classes instead of compatibility wrappers
creates broken or inaccurate Javadoc links and misleads callers about the actual
types and methods being invoked.

## Examples

Bad:

```cpp
/**
 * Calls {@link android.view.accessibility.AccessibilityNodeInfo#setSelection} if supported.
 *
 * @param info The accessibility node info to update.
 * @param selection The selection to set.
 */
default void setSelection(AccessibilityNodeInfoCompat info, @Nullable SelectionCompat selection) {
  // ...
}
```

Good:

```cpp
/**
 * Calls {@link android.view.accessibility.AccessibilityNodeInfoCompat#setSelection} if supported.
 *
 * @param info The accessibility node info to update.
 * @param selection The selection to set.
 */
default void setSelection(AccessibilityNodeInfoCompat info, @Nullable SelectionCompat selection) {
  // ...
}
```
