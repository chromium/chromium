---
name: general/phrase-javadoc-return-tags-as-descriptive-statements
rubric: readability
severity: nit
---

**Rule.** When documenting Java methods or interfaces with return values or
asynchronous outcomes (such as `Promise`), describe the return value,
fulfillment/rejection states, and return invariants in a dedicated `@return`
block tag rather than inlining return mechanics into the top-level summary.
Phrase `@return` tag descriptions as declarative clauses, conditions, or noun
phrases (e.g., '@return whether ...', '@return true if ...', '@return a promise
fulfilled when ...') rather than posing informal questions (e.g., '@return was
the request made?'), and keep them synchronized with the implementation.

**Why.** Placing return value contracts and failure/rejection modes in
structured `@return` tags keeps top-level method summaries concise and
standardizes API documentation for IDE tooltips and Javadoc generators. Phrasing
return tags as declarative statements rather than colloquial questions ensures
unambiguous caller contracts.

## Examples

Bad:

```cpp
/**
 * Calls the {@link android.app.ActivityManager.AppTask#requestWindowingLayer} method if
 * supported. If not supported, a promise rejected with {@link UnsupportedOperationException}
 * will be returned.
 *
 * @param appTask Task on which the method should be called.
 */
default Promise<Void> requestPinnedWindowingLayer(AppTask appTask);

/**
 * @return was the request made?
 */
default boolean requestTextCursorOnScreen(View view, Rect boundsInView);
```

Good:

```cpp
/**
 * Calls the {@link android.app.ActivityManager.AppTask#requestWindowingLayer} method.
 *
 * @param appTask Task on which the method should be called.
 * @return A promise fulfilled if going into pinned windowing layer is successful, rejected
 *     otherwise with {@link UnsupportedOperationException} if not supported.
 */
default Promise<Void> requestPinnedWindowingLayer(AppTask appTask);

/**
 * @return whether the Android API was invoked.
 */
default boolean requestTextCursorOnScreen(View view, Rect boundsInView);
```
