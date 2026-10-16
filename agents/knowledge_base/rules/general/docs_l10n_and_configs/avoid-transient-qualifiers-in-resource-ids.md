---
name: general/avoid-transient-qualifiers-in-resource-ids
rubric: readability
severity: should_fix
---

**Rule.** In `.grd` and `.grdp` resource files, do not use transient, temporal,
or versioned qualifiers (such as `MODERN`, `MODERNIZED`, `NEW`, `V2`, `REFRESH`,
`UPDATED`, or `REDESIGN`) in `<message>` resource identifiers (`IDS_*`). Name
resource IDs after the specific UI component, dialog, view, or surface (such as
`SIGN_IN_PROMO`, `BUBBLE`, or `SETTINGS_ROW`) and its semantic role instead.

**Why.** Temporal descriptors like 'modernized' or 'new' become stale,
confusing, and contradictory once UI updates become the standard baseline or
undergo subsequent iterations. Naming string IDs after the concrete UI surface,
component, or semantic role clearly communicates their purpose and prevents
ambiguity across multiple dialogs or views.

## Examples

Bad:

```cpp
<!-- In chrome/app/generated_resources.grd -->
<message name="IDS_SEND_TAB_TO_SELF_MODERNIZED_TITLE" desc="Title of the modernized send-tab-to-self signed-out sign-in promo bubble.">
  Sign in to Chrome
</message>
<message name="IDS_SETTINGS_NEW_SEARCH_ENGINE_CHOICE_LABEL" desc="Label for the new search engine choice screen in settings.">
  Choose your search engine
</message>
```

Good:

```cpp
<!-- In chrome/app/generated_resources.grd -->
<message name="IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_TITLE" desc="Title of the send-tab-to-self sign-in promo bubble.">
  Sign in to Chrome
</message>
<message name="IDS_SETTINGS_SEARCH_ENGINE_CHOICE_DIALOG_LABEL" desc="Label for the search engine choice dialog in settings.">
  Choose your search engine
</message>
```
