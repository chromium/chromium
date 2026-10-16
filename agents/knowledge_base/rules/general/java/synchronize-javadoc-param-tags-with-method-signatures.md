---
name: general/synchronize-javadoc-param-tags-with-method-signatures
rubric: readability
severity: nit
---

**Rule.** When documenting Java methods, constructors, or interfaces with
Javadoc comments, ensure each method parameter has a corresponding non-empty
@param <name> <description> tag that matches the exact parameter identifier and
casing. When method parameters are added, renamed, or removed during
refactoring, synchronize Javadoc comments by adding, updating, or deleting
matching @param tags accordingly.

**Why.** Missing or mismatched @param tags create incomplete or misleading API
documentation, degrade IDE tooltip accuracy, and violate Java documentation
conventions.

## Examples

Bad:

```cpp
/**
 * Sets text suggestion flags if supported.
 */
default void setTextFlagEnableTextSuggestionSelected(EditorInfo outAttrs) {}

/**
 * Requests focus for the specified input bounds.
 *
 * @param view View on which the method should be called.
 * @param rect The bounding rect to request on screen.
 */
default boolean requestInputFocusOnScreen(View view, Rect boundsInView) {
  return false;
}
```

Good:

```cpp
/**
 * Sets text suggestion flags if supported.
 *
 * @param outAttrs The EditorInfo object used to describe the attributes of the input connection.
 */
default void setTextFlagEnableTextSuggestionSelected(EditorInfo outAttrs) {}

/**
 * Requests focus for the specified input bounds.
 *
 * @param view View on which the method should be called.
 * @param boundsInView The bounding rect to request on screen.
 */
default boolean requestInputFocusOnScreen(View view, Rect boundsInView) {
  return false;
}
```
