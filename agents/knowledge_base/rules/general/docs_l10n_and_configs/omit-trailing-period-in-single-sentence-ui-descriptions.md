---
name: general/omit-trailing-period-in-single-sentence-ui-descriptions
rubric: readability
severity: nit
---

**Rule.** In `.grd` and `.grdp` resource files, do not end single-sentence
user-facing descriptions, subtitles, or sublabels (such as toggle summaries and
settings descriptions) with a trailing period. Use terminal periods only when
descriptive text contains multiple sentences or paragraphs.

**Why.** Chrome UI copy conventions omit terminal periods on single-sentence
descriptions and sublabels to reduce visual clutter and maintain a clean,
consistent presentation across WebUI and Views settings surfaces. Multi-sentence
descriptions retain standard sentence-ending punctuation for grammatical
clarity.

## Examples

Bad:

```cpp
<!-- Single-sentence description ending with a trailing period -->
<message name="IDS_SETTINGS_CPU_PERFORMANCE_OVERRIDE_DESCRIPTION" desc="Description for the CPU performance override setting.">
  Manually set the CPU performance tier reported to websites.
</message>
<message name="IDS_SETTINGS_MEMORY_SAVER_TOGGLE_SUBLABEL" desc="Sublabel for the memory saver toggle switch.">
  Frees up memory from inactive tabs.
</message>
```

Good:

```cpp
<!-- Single-sentence descriptions without trailing periods -->
<message name="IDS_SETTINGS_CPU_PERFORMANCE_OVERRIDE_DESCRIPTION" desc="Description for the CPU performance override setting.">
  Manually set the CPU performance tier reported to websites
</message>
<message name="IDS_SETTINGS_MEMORY_SAVER_TOGGLE_SUBLABEL" desc="Sublabel for the memory saver toggle switch.">
  Frees up memory from inactive tabs
</message>

<!-- Multi-sentence descriptions retain full punctuation -->
<message name="IDS_SETTINGS_PERFORMANCE_TAB_DISCARDING_DESCRIPTION" desc="Detailed description of tab discarding rules.">
  Tabs will not be discarded if on a site matching a rule in this list. Add sites you want to keep active.
</message>
```
