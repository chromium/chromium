---
name: general/sync-grd-platform-guards-and-descriptions
rubric: readability
severity: should_fix
---

**Rule.** When expanding or modifying platform coverage for localized <message>
strings in .grd and .grdp resource files, update the desc attribute to
accurately list all supported platforms and ensure platform conditional guards
(<if expr="...">) remain mutually exclusive across platform-specific string
variants. Do not remove or relax platform guards without guarding counterpart
variants or leave stale platform references in message descriptions.

**Why.** Outdated platform lists in message descriptions mislead translators and
code reviewers about where UI text is displayed and whether a string is still
actively used. Additionally, omitting or improperly broadening <if expr="...">
guards without maintaining mutually exclusive conditionals causes duplicate
message identifier conflicts on other platforms during resource compilation.

## Examples

Bad:

```cpp
<!-- In .grd / .grdp: -->
<if expr="is_macosx">
  <message name="IDS_SETTINGS_GLIC_OS_WIDGET_TOGGLE" desc="... (on Mac only).">
    Show Gemini in Mac menu bar and turn on keyboard shortcut
  </message>
</if>
<!-- Stale desc omitting ChromeOS, and missing mutually exclusive guard: -->
<message name="IDS_SETTINGS_GLIC_OS_WIDGET_TOGGLE" desc="Name of the toggle ... (on Windows and Linux).">
  Show Gemini in system tray and turn on keyboard shortcut
</message>
```

Good:

```cpp
<!-- In .grd / .grdp: -->
<if expr="is_macosx">
  <message name="IDS_SETTINGS_GLIC_OS_WIDGET_TOGGLE" desc="... (on Mac only).">
    Show Gemini in Mac menu bar and turn on keyboard shortcut
  </message>
</if>
<if expr="is_win or is_linux or is_chromeos">
  <message name="IDS_SETTINGS_GLIC_OS_WIDGET_TOGGLE" desc="Name of the toggle ... (on ChromeOS, Windows and Linux).">
    Show Gemini in system tray and turn on keyboard shortcut
  </message>
</if>
```
