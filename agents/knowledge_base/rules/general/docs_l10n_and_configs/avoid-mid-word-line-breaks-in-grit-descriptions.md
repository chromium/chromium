---
name: general/avoid-mid-word-line-breaks-in-grit-descriptions
rubric: readability
severity: nit
---

**Rule.** In `.grd` and `.grdp` resource files, do not break lines in the middle
of words within message description (`desc`) attributes. Keep the `desc` string
on a single line or break lines cleanly between words at whitespace boundaries.

**Why.** GRIT XML files are formatted manually because `git cl format` does not
format XML. Breaking lines in the middle of words hurts readability in code
reviews and risks introducing unintentional whitespace, hyphenation artifacts,
or corrupted words in translation toolchains and localization descriptions.

## Examples

Bad:

```cpp
<message name="IDS_SETTINGS_AUTOFILL_AI_WHEN_ON_SAVED_INFO" desc="Bullet 1 of 2 that appears beneath the 'When on' title. 'Autofill': the user is on the 'enha
nced autofill' feature page, but we want to frame this new type of autofill as just 'Autofill'...">
  Autofill can offer to save info when you submit forms, like your driver's license or passport number
</message>
```

Good:

```cpp
<message name="IDS_SETTINGS_AUTOFILL_AI_WHEN_ON_SAVED_INFO" desc="Bullet 1 of 2 that appears beneath the 'When on' title. 'Autofill': the user is on the 'enhanced autofill' feature page, but we want to frame this new type of autofill as just 'Autofill'...">
  Autofill can offer to save info when you submit forms, like your driver's license or passport number
</message>
```
