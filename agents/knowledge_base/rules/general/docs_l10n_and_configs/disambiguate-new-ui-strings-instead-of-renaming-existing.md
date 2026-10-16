---
name: general/disambiguate-new-ui-strings-instead-of-renaming-existing
rubric: readability
severity: should_fix
---

**Rule.** When introducing new UI strings that overlap, conflict, or share
context with existing strings in `.grd` or `.grdp` files, disambiguate the newly
added string resource ID (e.g., using a feature, context, or platform suffix
like `_ANDROID`) or reuse the existing resource in a shared file. Do not rename
existing, established UI string IDs to resolve naming conflicts with new
strings.

**Why.** Renaming established string IDs creates unnecessary churn across C++
and Java callers, invalidates existing translation screenshot hashes
(`.png.sha1`), and disrupts paired branded resource files (such as
`chromium_strings.grd` and `google_chrome_strings.grd`) where string IDs must
match identically across Chromium and Google Chrome builds.

## Examples

Bad:

```cpp
<!-- In chrome/app/chromium_strings.grd: Renaming existing established ID -->
- <message name="IDS_SETTINGS_GLIC_PERMISSIONS_WEB_ACTUATION_TOGGLE" desc="...">
+ <message name="IDS_SETTINGS_GLIC_CHROMIUM_PERMISSIONS_WEB_ACTUATION_TOGGLE" desc="...">
    Let Chromium browse for you
  </message>
<!-- In chrome/app/glic_strings.grdp: Adding conflicting generic ID -->
+ <message name="IDS_SETTINGS_GLIC_PERMISSIONS_WEB_ACTUATION_TOGGLE" formatter_data="android_java" desc="...">
+   Let Chrome browse for you
+ </message>
```

Good:

```cpp
<!-- In chrome/app/chromium_strings.grd: Retain original string ID -->
<message name="IDS_SETTINGS_GLIC_PERMISSIONS_WEB_ACTUATION_TOGGLE" desc="...">
  Let Chromium browse for you
</message>
<!-- In chrome/app/glic_strings.grdp: Disambiguate newly added string ID -->
<message name="IDS_SETTINGS_GLIC_PERMISSIONS_CHROME_WEB_ACTUATION_TOGGLE" formatter_data="android_java" desc="...">
  Let Chrome browse for you
</message>
```
