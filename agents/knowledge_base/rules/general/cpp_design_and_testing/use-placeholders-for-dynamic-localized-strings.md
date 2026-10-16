---
name: general/use-placeholders-for-dynamic-localized-strings
rubric: testing
severity: should_fix
---

**Rule.** Do not hardcode dynamic, configurable, or platform-varying values
(such as size limits, quotas, timeouts, counts, or keyboard shortcuts) directly
into .grd or .grdp resource strings, and do not concatenate numbers or variables
with localized strings in code or templates. Define messages with <ph>
placeholders (including representative <ex> sample values) and inject formatted
values dynamically using localization APIs such as l10n_util::GetStringF\*
(e.g., accelerator.GetShortcutText() or base::FormatNumber()) or WebUI $i18n
substitutions.

**Why.** Hardcoding limits, counts, or shortcuts into translation resources
forces unnecessary re-translations across all locales when configurations
change, risks showing stale or inaccurate values, and prevents platform
adaptation. Concatenating dynamic values directly with localized strings forces
English word order and grammatical structures, breaking translations in
languages with different syntax or bidirectional text.

## Examples

Bad:

```cpp
<!-- In .grd / .grdp (hardcoded limit or shortcut): -->
<message name="IDS_FILE_TOO_LARGE" desc="Error when upload exceeds limit.">
  Can't upload. File exceeds maximum supported size of 100MB.
</message>
<message name="IDS_READING_MODE_TOOLTIP" desc="Tooltip for reading mode.">
  Reading mode (Alt+Shift+R)
</message>

<!-- In WebUI template (string concatenation): -->
<div class="error-message">
  ${MAX_PROMPT_CHAR_COUNT} $i18n{charLimitError}
</div>
```

Good:

```cpp
<!-- In .grd / .grdp (dynamic <ph> placeholders): -->
<message name="IDS_FILE_TOO_LARGE" desc="Error when upload exceeds limit.">
  Can't upload. File exceeds maximum supported size of <ph name="MAX_FILE_SIZE">$1<ex>100</ex></ph> MB.
</message>
<message name="IDS_READING_MODE_TOOLTIP" desc="Tooltip for reading mode.">
  Reading mode (<ph name="READING_MODE_SHORTCUT">$1<ex>Alt+Shift+R</ex></ph>)
</message>

// In C++ (injecting formatted values):
l10n_util::GetStringFUTF16(IDS_READING_MODE_TOOLTIP,
                           accelerator.GetShortcutText());
source->AddString("charLimitError", l10n_util::GetStringFUTF16(
    IDS_CHAR_LIMIT_ERROR, base::FormatNumber(kMaxPromptCharCount)));
```
