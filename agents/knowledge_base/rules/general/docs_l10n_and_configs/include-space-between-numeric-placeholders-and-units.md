---
name: general/include-space-between-numeric-placeholders-and-units
rubric: testing
severity: should_fix
---

**Rule.** In .grd and .grdp resource files, insert a space between numeric
placeholders (<ph>) and subsequent unit abbreviations (such as 'B', 'KB', 'MB',
'GB', 'TB', 'min', 'hr', or 's') rather than attaching the unit directly to the
closing </ph> tag. If UI mockups or design screenshots show the unit attached
without a space, update the translation screenshot and UI to include the space
rather than omitting it from the localized resource string.

**Why.** Standard English typography and Chromium UI consistency require a
whitespace delimiter between numeric quantities and abbreviated units. Omitting
spaces creates cramped, non-standard typography and can cause translation
ambiguities across localized locales where number and unit formatting
conventions differ.

## Examples

Bad:

```cpp
<!-- Bad: Unit abbreviation attached directly to placeholder without a space -->
<message name="IDS_FILE_UPLOAD_TOO_LARGE" desc="Error when uploaded file exceeds size limit.">
  File exceeds maximum supported size of <ph name="MAX_FILE_SIZE">$1<ex>100</ex></ph>MB.
</message>
```

Good:

```cpp
<!-- Good: Space between numeric placeholder and unit abbreviation -->
<message name="IDS_FILE_UPLOAD_TOO_LARGE" desc="Error when uploaded file exceeds size limit.">
  File exceeds maximum supported size of <ph name="MAX_FILE_SIZE">$1<ex>100</ex></ph> MB.
</message>
```
