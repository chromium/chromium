---
name: general/avoid-asymmetric-icu-plurals-and-simplify-nounless-counts
rubric: testing
severity: should_fix
---

**Rule.** In .grd and .grdp resource files, use ICU plural format
(`{COUNT, plural, =1 {...} other {# ...}}`) for strings with quantities instead
of creating separate singular and plural <message> definitions and switching
between them in application code. Conversely, avoid asymmetric plural branches
that omit the noun in one branch, and do not use ICU plural branching when the
noun is omitted across all quantities (such as compact '+# more' count
indicators); use a single <ph> placeholder instead.

**Why.** Pluralization rules differ across human languages: many languages have
more than two plural forms or non-English plural boundaries, so hardcoding
binary singular/plural checks in C++ or TypeScript breaks localization.
Conversely, asymmetric branches or unnecessary ICU branching on uninflected
counts create confusing translation instructions and resource bloat without
linguistic benefit.

## Examples

Bad:

```cpp
<!-- Bad: Separate singular/plural messages switched in application code -->
<message name="IDS_SETTINGS_CPU_PERFORMANCE_CORE" desc="Label showing a single CPU core.">
  1 core
</message>
<message name="IDS_SETTINGS_CPU_PERFORMANCE_CORES" desc="Label showing multiple CPU cores.">
  <ph name="CPU_CORE_COUNT">$1<ex>8</ex></ph> cores
</message>

<!-- Bad: Asymmetric branches or nounless ICU plural -->
<message name="IDS_TAB_GROUPS_HOVER_CARD_FOOTER" desc="The number of tabs not shown on the hover card.">
  {NUM_TABS, plural, =1 {(+1 more)} other {(# Tabs more)}}
</message>
```

Good:

```cpp
<!-- Good: Unified ICU plural message resolved via l10n_util::GetPluralStringF* -->
<message name="IDS_SETTINGS_CPU_PERFORMANCE_CORES" desc="Label showing number of CPU cores.">
  {CORE_COUNT, plural,
    =1 {1 core}
    other {{CORE_COUNT} cores}}
</message>

<!-- Good: Simple parameterized placeholder for nounless counts -->
<message name="IDS_TAB_GROUPS_HOVER_CARD_FOOTER" desc="The number of tabs not shown on the hover card.">
  +<ph name="NUM">$1<ex>2</ex></ph> more
</message>
```
