---
name: general/avoid-including-large-string-bundles-for-few-strings
rubric: safety
severity: should_fix
---

**Rule.** Do not expand <if expr="..."> build guards to include large
platform-excluded string or resource bundles (.grdp files) when only a few
strings are needed. Instead, move or extract the required string(s) into a
feature-specific or shared resource bundle (.grdp) to avoid packaging hundreds
of unused translated strings into target builds.

**Why.** Large resource bundles (such as settings_strings.grdp) define hundreds
or thousands of localized strings that are translated and packaged into .pak
files across all supported locales. Including an entire bundle to satisfy a
dependency on a single string causes significant PAK and APK/binary size bloat,
particularly on resource-constrained platforms like Android.

## Examples

Bad:

```cpp
<!-- In chrome/app/generated_resources.grd -->
<if expr="not is_android or enable_extensions_core">
  <part file="settings_strings.grdp" />
  <part file="shared_settings_strings.grdp" />
</if>
```

Good:

```cpp
<!-- In chrome/app/generated_resources.grd -->
<if expr="not is_android">
  <part file="settings_strings.grdp" />
  <part file="shared_settings_strings.grdp" />
</if>

<!-- In components/management_strings.grdp -->
<message name="IDS_MANAGEMENT_SEARCH_PROMPT" desc="Label for input field to search within settings.">
  Search settings
</message>
```
