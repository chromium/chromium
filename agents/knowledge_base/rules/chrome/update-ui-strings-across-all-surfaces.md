---
name: chrome/update-ui-strings-across-all-surfaces
rubric: correctness
severity: should_fix
---

**Rule.** When updating, replacing, or rebranding UI string resources for a
feature, audit and update all counterpart entry points and presentation surfaces
(such as native Views controllers, context menus, and WebUI data handlers) that
display the feature. Do not update string IDs or copy in only one surface while
leaving counterpart controllers displaying outdated or inconsistent text.

**Why.** Features such as search boxes, tab pickers, and side panels are
frequently presented across both native UI controllers and WebUI page handlers.
Updating copy in only one surface leaves other entry points with inconsistent,
outdated, or confusing user-facing text.

## Examples

Bad:

```cpp
// Updating string ID in WebUI handler only:
void SearchboxHandler::SetupWebUIDataSource(content::WebUIDataSource* source) {
  source->AddLocalizedString("addTab", IDS_NTP_COMPOSEBOX_TAB_PICKER_ADD_TABS_TITLE);
}

// Leaving native controller referencing legacy string:
void OmniboxContextMenuController::AddRecentTabItems() {
  if (omnibox::kShowContextMenuHeaders.Get()) {
    AddTitleWithStringId(IDS_NTP_COMPOSE_MOST_RECENT_TABS);
  }
}
```

Good:

```cpp
// Updating string ID across both WebUI and native controllers:
void SearchboxHandler::SetupWebUIDataSource(content::WebUIDataSource* source) {
  source->AddLocalizedString("addTab", IDS_NTP_COMPOSEBOX_TAB_PICKER_ADD_TABS_TITLE);
}

void OmniboxContextMenuController::AddRecentTabItems() {
  if (omnibox::kShowContextMenuHeaders.Get()) {
    AddTitleWithStringId(IDS_NTP_COMPOSEBOX_TAB_PICKER_ADD_TABS_TITLE);
  }
}
```
