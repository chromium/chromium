---
name: general/mirror-underlying-api-parameter-names-in-wrappers
rubric: readability
severity: should_fix
---

**Rule.** When defining delegate, wrapper, adapter, or shim methods that forward
to or wrap underlying platform, OS, or specification APIs, mirror parameter
names, terminology, and parameter documentation from official platform contracts
(e.g., using `offset` rather than `index`, or `descendantId` rather than
`viewId`). However, avoid documenting overly narrow parameter bounds or domain
constraints (such as stating offsets are strictly bounded between 0 and text
length) when the API supports or is intended to support non-text elements or
broader representations.

**Why.** Diverging from underlying platform or specification parameter names
creates ambiguity and cognitive overhead when cross-referencing upstream
documentation. Conversely, copying overly narrow or restrictive domain
constraints into doc comments artificially limits extensible interfaces and
misleads callers when APIs handle broader representations.

## Examples

Bad:

```cpp
// Anti-pattern: Divergent parameter names ('startIndex') or overly narrow bounds ('between 0 and text length')
public interface AconfigFlaggedApiDelegate {
    /**
     * @param view The view to which `info` is associated.
     * @param startOffset The offset for a selection position within the start virtual descendant's
     *     text content, which should be a value between 0 and the length of the descendant's text.
     */
    default void setSelection(
            AccessibilityNodeInfoCompat info,
            android.view.View view,
            int startVirtualViewId,
            int startIndex,
            int endVirtualViewId,
            int endIndex) {}
}
```

Good:

```cpp
// Idiomatic: Mirror platform parameter names and avoid overly narrow domain bounds on extensible APIs
public interface AconfigFlaggedApiDelegate {
    /**
     * @param view The view whose virtual descendant is associated with the selection position.
     * @param startOffset The offset for a selection position within the start virtual descendant's
     *     text content.
     */
    default void setSelection(
            AccessibilityNodeInfoCompat info,
            android.view.View view,
            int startVirtualDescendantId,
            int startOffset,
            int endVirtualDescendantId,
            int endOffset) {}
}
```
