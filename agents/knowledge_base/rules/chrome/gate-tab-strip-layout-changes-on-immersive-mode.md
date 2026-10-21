---
name: chrome/gate-tab-strip-layout-changes-on-immersive-mode
rubric: correctness
severity: should_fix
---

**Rule.** When adding or modifying UI entry points (such as menu items,
commands, or toolbar controls) that change tab strip or window frame layouts,
verify window state using ImmersiveModeController::IsEnabled() and hide or
disable the action in immersive mode. Do not expose layout-switching controls
(such as toggling vertical tabs) when immersive fullscreen mode is active.

**Why.** Window frame and tab strip layout modifications (such as toggling
between horizontal and vertical tab strips) can cause visual corruption,
duplicate tab strips, or broken chrome animations when executed during immersive
fullscreen mode. Gating layout actions on ImmersiveModeController::IsEnabled()
ensures commands are only available when the window layout supports the
transition.

## Examples

Bad:

```cpp
if (controller) {
  AddSeparator(ui::NORMAL_SEPARATOR);
  if (controller->ShouldDisplayVerticalTabs()) {
    AddItemWithStringId(TabStripModel::CommandToggleVertical,
                        IDS_SWITCH_TO_HORIZONTAL_TAB);
  }
}
```

Good:

```cpp
if (controller) {
  BrowserWindowInterface* bwi =
      tab_strip->delegate()->GetBrowserWindowInterface();
  if (bwi && bwi->GetFeatures().immersive_mode_controller()->IsEnabled()) {
    return;
  }
  AddSeparator(ui::NORMAL_SEPARATOR);
  if (controller->ShouldDisplayVerticalTabs()) {
    AddItemWithStringId(TabStripModel::CommandToggleVertical,
                        IDS_SWITCH_TO_HORIZONTAL_TAB);
  }
}
```
