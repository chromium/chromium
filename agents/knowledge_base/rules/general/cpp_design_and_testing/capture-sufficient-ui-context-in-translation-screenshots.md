---
name: general/capture-sufficient-ui-context-in-translation-screenshots
rubric: testing
severity: should_fix
---

**Rule.** When adding or updating translation screenshot hash files
(\*.png.sha1), do not create screenshot hashes for untranslated strings (such as
prototype strings or those marked translateable="false"), and do not overwrite
an existing platform's established screenshot when sharing strings across
platforms (e.g., adding an Android variant to a desktop string). For new or
modified translatable strings, verify that SHA1 hashes correspond to valid
screenshots uploaded to Google Cloud Storage via
tools/translation/upload_screenshots.py rather than dummy or placeholder hashes,
capture sufficient surrounding window context rather than tightly cropped
controls, and ensure the captured UI text accurately matches the .grd or .grdp
string definition.

**Why.** Translation screenshots provide essential visual and semantic context
to human translators during localization. Untranslated strings do not require
screenshot hashes, while overwriting established desktop screenshots when adding
mobile variants erases visual context for desktop translators. Unuploaded or
dummy hashes break localization tooling, tightly cropped images obscure visual
hierarchy, and outdated or mismatched text leads to inaccurate localizations
that are costly to fix.

## Examples

Bad:

```cpp
# 1. Adding screenshot for untranslated string (translateable="false"):
# chrome/app/generated_resources_grd/IDS_NTP_COMPOSE_ADD_DRIVE.png.sha1
34c82ebeba07ca0c314b1f4a09d0478240997b18

# 2. Overwriting existing desktop screenshot when sharing string with Android:
# chrome/app/glic_strings_grdp/IDS_SETTINGS_GLIC_REVOKE_ACTOR_LOGIN_PERMISSION_ARIA_LABEL.png.sha1
-cfc719902e25d704700701511673cdd270c686c4  # Existing desktop screenshot
+6a1f274591ed728dacfb32d2f832ab92734a0248  # Overwriting with Android screenshot

# 3. Unuploaded/dummy hash (40 zeros) or tightly cropped/mismatched screenshot:
# chrome/app/settings_strings_grdp/IDS_SETTINGS_CTRL_TAB_MRU.png.sha1
0000000000000000000000000000000000000000
```

Good:

```cpp
# 1. Omit screenshot file for untranslated strings:
<!-- <message name="IDS_NTP_COMPOSE_ADD_DRIVE" translateable="false">...</message> -->
# (No IDS_NTP_COMPOSE_ADD_DRIVE.png.sha1 file added)

# 2. Retain existing platform screenshot intact when adding cross-platform variants:
# chrome/app/glic_strings_grdp/IDS_SETTINGS_GLIC_REVOKE_ACTOR_LOGIN_PERMISSION_ARIA_LABEL.png.sha1
cfc719902e25d704700701511673cdd270c686c4  # Retain original desktop screenshot

# 3. Valid Cloud Storage SHA1 hash matching string text with broad UI context:
# chrome/app/settings_strings_grdp/IDS_SETTINGS_SITE_SETTINGS_PARENT_APP_PERMISSION_EXPLANATION.png.sha1
58cfdf7bb75a87769cbc888de8146aec6bd999d8
```
