---
name: general/omit-titlecase-prefix-from-sentencecase-grit-descriptions
rubric: testing
severity: should_fix
---

**Rule.** When defining UI strings in .grd or .grdp resource files that require
platform-specific casing (such as Title Case on macOS vs sentence case on
Windows and Linux), branch visible messages using <if expr="use_titlecase"> (or
<then>) and <if expr="not use_titlecase"> (or <else>), and prefix only the
title-case description with In Title Case:. Do not branch on use_titlecase for
non-visible accessibility strings (such as messages with
is_accessibility_with_no_ui="true"); define accessibility strings
unconditionally as platform title casing applies only to visible UI controls.

**Why.** macOS UI guidelines specify title case for visible controls and short
labels, while Windows, Linux, and ChromeOS use sentence case. Prepending In
Title Case: instructs translators to capitalize target locale strings
appropriately, but retaining the prefix on sentence-case variants causes
improper capitalization. Conversely, platform title casing does not apply to
non-visible accessibility strings; branching on use_titlecase for them creates
unnecessary translation duplicates and localization churn without UI benefit.

## Examples

Bad:

```cpp
<!-- Non-visible accessibility string unnecessarily branched on use_titlecase -->
<if expr="use_titlecase">
  <then>
    <message name="IDS_VERTICAL_RESIZE_AREA" is_accessibility_with_no_ui="true" desc="In Title Case: Accessibility label for resize area.">
      Vertical Tab Strip Resize Handle (Draggable)
    </message>
  </then>
  <else>
    <message name="IDS_VERTICAL_RESIZE_AREA" is_accessibility_with_no_ui="true" desc="Accessibility label for resize area.">
      Vertical tab strip resize handle (draggable)
    </message>
  </else>
</if>

<!-- Misleading 'In Title Case:' prefix retained on sentence-case branch -->
<if expr="use_titlecase">
  <message name="IDS_TAB_GROUPS_HOVER_CARD" desc="In Title Case: Header label.">
    Tab Group (1 Tab)
  </message>
</if>
<if expr="not use_titlecase">
  <message name="IDS_TAB_GROUPS_HOVER_CARD" desc="In Title Case: Header label.">
    Tab group (1 tab)
  </message>
</if>
```

Good:

```cpp
<!-- Non-visible accessibility string defined unconditionally -->
<message name="IDS_VERTICAL_RESIZE_AREA" is_accessibility_with_no_ui="true" desc="Accessibility label for resize area.">
  Vertical Tab Strip Resize Handle (Draggable)
</message>

<!-- Visible UI control branched on use_titlecase with prefix on title-case branch only -->
<if expr="use_titlecase">
  <then>
    <message name="IDS_TAB_GROUP_OPTIONS" desc="In Title Case: Options button label.">
      More Options
    </message>
  </then>
  <else>
    <message name="IDS_TAB_GROUP_OPTIONS" desc="Options button label.">
      More options
    </message>
  </else>
</if>
```
