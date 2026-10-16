---
name: general/omit-inapplicable-or-unchecked-javadoc-throws
rubric: readability
severity: should_fix
---

**Rule.** When writing Javadoc comments for Java methods, interfaces, or
delegate wrappers, do not include `@throws` tags for unchecked runtime
exceptions (such as `IllegalStateException` or `NullPointerException`) unless
they define an actionable caller contract. Avoid copying platform or Android
framework Javadoc `@throws` tags verbatim when the documented failure conditions
are internal to the platform, inapplicable to Chromium callers, or irrelevant to
the delegate's scope.

**Why.** Unchecked exceptions represent runtime programming bugs or internal
state failures rather than recoverable caller conditions, so tagging them adds
boilerplate noise without actionable value. Copying platform Javadoc verbatim
often introduces confusing constraints (such as accessibility service
requirements) that do not apply within Chrome, misleading maintainers and
callers.

## Examples

Bad:

```cpp
/**
 * Calls {@link android.view.accessibility.AccessibilityNodeInfo#setSelection} if supported.
 *
 * @param selection The selection, or {@code null} to clear the selection.
 * @throws IllegalStateException If called from an AccessibilityService.
 */
default void setSelection(AccessibilityNodeInfoCompat info, @Nullable Selection selection) {}
```

Good:

```cpp
/**
 * Calls {@link android.view.accessibility.AccessibilityNodeInfo#setSelection} if supported.
 *
 * @param info The accessibility node info to update.
 * @param selection The selection, or {@code null} to clear the selection.
 */
default void setSelection(AccessibilityNodeInfoCompat info, @Nullable Selection selection) {}
```
