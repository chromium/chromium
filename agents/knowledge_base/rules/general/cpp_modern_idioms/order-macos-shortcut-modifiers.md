---
name: general/order-macos-shortcut-modifiers
rubric: readability
severity: should_fix
---

**Rule.** When displaying macOS keyboard shortcuts in user-facing UI copy,
sample placeholders (<ex>), or string resources, order modifier keys according
to standard macOS convention: Control (⌃), Option (⌥), Shift (⇧), Command (⌘).
Specifically, place Option (⌥) before Command (⌘), and avoid reverse orderings
like Command + Option or ⌘⌥.

**Why.** Apple Human Interface Guidelines and Chromium UI standards require a
consistent modifier order of Control, Option, Shift, and Command across all
menus, accelerators, and user-facing copy. Deviating from standard macOS
modifier ordering creates visual inconsistency with system menus and other
browser surfaces.

## Examples

Bad:

```cpp
<!-- In GRD / UI resource files or WebUI copy -->
<message name="IDS_READING_MODE_PROMO" desc="Promo text for reading mode shortcut">
  Open reading mode faster with a shortcut. Press ⌘ + ⌥ + R to toggle reading mode.
</message>
<message name="IDS_READING_MODE_MENU_ITEM" desc="...">
  Reading mode (<ph name="SHORTCUT">$1<ex>⌘⌥R</ex></ph>)
</message>
```

Good:

```cpp
<!-- In GRD / UI resource files or WebUI copy -->
<message name="IDS_READING_MODE_PROMO" desc="Promo text for reading mode shortcut">
  Open reading mode faster with a shortcut. Press ⌥ + ⌘ + R to toggle reading mode.
</message>
<message name="IDS_READING_MODE_MENU_ITEM" desc="...">
  Reading mode (<ph name="SHORTCUT">$1<ex>⌥⌘R</ex></ph>)
</message>
```
