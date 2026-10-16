---
name: general/guard-branded-strings-in-grit-with-google-chrome-expr
rubric: testing
severity: should_fix
---

**Rule.** In Grit resource files (`.grd`, `.grdp`):

1. **Brand-neutral strings**: Place UI string definitions that do not reference
   brand names in shared resource files (such as `generated_resources.grd`,
   `components_strings.grd`, or `ios_strings.grd`) rather than duplicating
   identical strings across branded resource files (`chromium_strings.grd` and
   `google_chrome_strings.grd`).
2. **Brand-specific strings**: Do not hardcode product names (such as 'Chrome',
   'Chromium', or 'Google Chrome') in unbranded or shared message definitions.
   Instead, split messages across product-specific branded files
   (`chromium_strings.grd` and `google_chrome_strings.grd`), guard variants
   using `<if expr="_google_chrome">` with `<then>` and `<else>` blocks, use
   `<ph name="PRODUCT_NAME">` placeholders, or omit the product name if generic
   wording suffices.
3. **Copy parity**: When defining counterpart branded messages across paired
   files or conditional branches, ensure the text remains strictly identical in
   wording, punctuation, and capitalization, differing only in the product name.

**Why.** Chromium is built in both branded (Google Chrome) and unbranded
(Chromium) configurations. Hardcoding branded product names in shared string
resources causes unbranded builds to display incorrect branding, while
duplicating brand-neutral strings across branded GRD files causes unnecessary
redundancy and complicates translation maintenance. Furthermore, divergences in
punctuation, capitalization, or phrasing across counterpart branded messages
create inconsistent UI behavior and localization discrepancies across
distributions.

## Examples

Bad:

```cpp
<!-- In chrome/app/generated_resources.grd (shared/unbranded): hardcoded brand name -->
<message name="IDS_DOWNLOAD_BY_EXTENSION_URL" desc="...">
  Downloaded by Chrome extension: &lt;a href=&quot;<ph name="URL">$1<ex>adblock.com</ex></ph>&quot;&gt;<ph name="EXTENSION">$2<ex>Extension Name</ex></ph>&lt;/a&gt;
</message>

<!-- In chrome/app/chromium_strings.grd and google_chrome_strings.grd (brand-neutral strings duplicated across branded files) -->
<message name="IDS_DEFAULT_BROWSER_DIALOG_OPEN_SETTINGS_LABEL" desc="Label for the 'Open settings' button.">
  Open settings
</message>

<!-- In google_chrome_strings.grd and chromium_strings.grd (divergent punctuation/casing) -->
<!-- google_chrome_strings.grd -->
<message name="IDS_STARTUP_LAUNCH_INFOBAR_OPT_IN_TITLE" desc="...">
  Begin browsing instantly: Chrome can now launch when Windows starts.
</message>
<!-- chromium_strings.grd -->
<message name="IDS_STARTUP_LAUNCH_INFOBAR_OPT_IN_TITLE" desc="...">
  Begin browsing instantly. Chromium can now launch when windows starts.
</message>
```

Good:

```cpp
<!-- In chrome/app/generated_resources.grd (brand-neutral strings and generic wording) -->
<message name="IDS_DEFAULT_BROWSER_DIALOG_OPEN_SETTINGS_LABEL" desc="Label for the 'Open settings' button.">
  Open settings
</message>
<message name="IDS_DOWNLOAD_BY_EXTENSION_URL" desc="...">
  Downloaded by extension: &lt;a href=&quot;<ph name="URL">$1<ex>adblock.com</ex></ph>&quot;&gt;<ph name="EXTENSION">$2<ex>Extension Name</ex></ph>&lt;/a&gt;
</message>

<!-- In chrome/app/chromium_strings.grd and google_chrome_strings.grd (split branded files with copy parity) -->
<!-- chromium_strings.grd -->
<message name="IDS_STARTUP_LAUNCH_INFOBAR_OPT_IN_TITLE" desc="...">
  Begin browsing instantly: Chromium can now launch when Windows starts.
</message>
<!-- google_chrome_strings.grd -->
<message name="IDS_STARTUP_LAUNCH_INFOBAR_OPT_IN_TITLE" desc="...">
  Begin browsing instantly: Chrome can now launch when Windows starts.
</message>

<!-- Or inline via <if expr="_google_chrome"> in shared files -->
<if expr="_google_chrome">
  <then>
    <message name="IDS_USER_INTERVENTION_TITLE" desc="...">
      Chrome needs help changing your password
    </message>
  </then>
  <else>
    <message name="IDS_USER_INTERVENTION_TITLE" desc="...">
      Chromium needs help changing your password
    </message>
  </else>
</if>
```
