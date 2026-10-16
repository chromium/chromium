---
name: general/mark-prototype-strings-not-translateable
rubric: testing
severity: should_fix
---

**Rule.** In `.grd` and `.grdp` resource files, keep user-facing UI strings
translatable by omitting `translateable` attributes entirely, as messages are
translatable by default in GRIT; never explicitly specify redundant
`translateable="true"`. Reserve `translateable="false"` strictly for strings
explicitly intended to remain untranslated (such as brand names, technical
identifiers, and `chrome://flags` entries) or prototype/provisional strings
awaiting finalization (to defer translation screenshots); when marking
provisional strings non-translatable during development, ensure a tracking bug
is filed to finalize copy and enable translation before launch.

**Why.** Messages in GRIT default to `translateable="true"`, making explicit
`translateable="true"` attributes redundant boilerplate. Conversely, marking
finalized user-facing strings with `translateable="false"` excludes them from
localization, while omitting `translateable="false"` on unfinalized prototype
strings prematurely triggers translation screenshot requirements and
localization churn without a tracking issue to ensure they are finalized and
translated prior to release.

## Examples

Bad:

```cpp
<!-- Bad: Explicit redundant translateable="true" (messages are translatable by default) -->
<message name="IDS_SKILL_SAVED_TOAST_BODY" desc="Text on the toast after a skill is saved" translateable="true">
  Saved skill
</message>

<!-- Bad: Finalized user-facing UI string marked untranslatable -->
<message name="IDS_REPORT_UNSAFE_SITE_DIALOG_DESCRIPTION" desc="Explanatory text in dialog for reporting an unsafe site to Google" translateable="false">
  Improve online safety for everyone when you report a site you think is harmful.
</message>

<!-- Bad: Provisional string omitting translateable="false" (prematurely triggers screenshot requirement) -->
<message name="IDS_SETTINGS_GLIC_ACTOR_LOGIN_PERMISSIONS_SECTION_TITLE" desc="Title for the Gemini web login linkout.">
  Sites Gemini can sign you in to
</message>
```

Good:

```cpp
<!-- Good: Translatable user-facing UI string omits translateable attribute -->
<message name="IDS_SKILL_SAVED_TOAST_BODY" desc="Text on the toast after a skill is saved">
  Saved skill
</message>

<!-- Good: Provisional or prototype string marked translateable="false" during development, tracked by a bug to finalize and translate before release -->
<message name="IDS_SETTINGS_GLIC_ACTOR_LOGIN_PERMISSIONS_SECTION_TITLE" desc="Title for the Gemini web login linkout." translateable="false">
  Sites Gemini can sign you in to
</message>
```
