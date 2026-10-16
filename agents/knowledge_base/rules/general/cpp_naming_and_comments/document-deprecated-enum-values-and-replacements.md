---
name: general/document-deprecated-enum-values-and-replacements
rubric: readability
severity: nit
---

**Rule.** When deprecating or superseding legacy enum values, fields, or
constants in favor of more granular entries, add an explicit doc comment to
every obsolete entry explaining that it should no longer be used and directing
consumers to the replacement. Do not annotate only a subset of superseded
entries while leaving adjacent obsolete entries undocumented.

**Why.** Leaving some superseded enum values or constants undocumented while
annotating adjacent entries creates ambiguity about whether the unannotated
entries are still actively supported. Explicitly documenting each obsolete value
and its granular replacement prevents accidental usage and clarifies the
migration path for schema consumers.

## Examples

Bad:

```cpp
// Superseding legacy catch-all entries with granular values:
// This entry should no longer be used. This was a catch-all for all methods
// in the WebView instance but we now have granular callsites for each method.
WEBVIEW_INSTANCE = 2;
GET_STATICS = 3;  // Obsolete catch-all left undocumented
```

Good:

```cpp
// Document each superseded entry individually with its replacement:
// This entry should no longer be used. This was a catch-all for all methods
// in the WebView instance but we now have granular callsites for each method.
WEBVIEW_INSTANCE = 2;
// This entry should no longer be used. This was a catch-all for all static
// methods but we now have granular callsites for each method.
GET_STATICS = 3;
```
