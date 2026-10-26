---
name: geic/verify-tab-ownership-and-focus-before-reuse-or-close
rubric: correctness
severity: should_fix
---

**Rule.** When reusing or closing a browser tab managed by GEiC
(`GeicManagedTab`), always retrieve `tabs::TabInterface* tab = tab_.Get()`,
verify that `tab` is non-null, and check `IsStillOwned(tab)` to confirm it has
not navigated away from expected origins before reusing or closing it. In
`GeicManagedTab::Close()`, clear `tab_` and `tab_active_before_open_` before
closing the tab, and only restore focus via
`TabListInterface::From(browser)->ActivateTab(restore_to)` if the managed tab
was active (`tab->IsActivated()`).

**Why.** A user may manually navigate an auth tab to an unrelated site or switch
focus to another tab while the auth tab is open. Reusing or closing a tab that
is no longer on an expected origin clobbers or destroys user-owned browsing
state, and unconditionally activating `tab_active_before_open_` steals focus
from whatever tab the user is actively using.

## Examples

Bad:

```cpp
GeicManagedTab::CloseOutcome GeicManagedTab::Close() {
  tabs::TabInterface* tab = tab_.Get();
  if (!tab) {
    return CloseOutcome::kAlreadyClosed;
  }
  // Closes the tab even if the user navigated it to another site, and
  // unconditionally forces focus back to `tab_active_before_open_`:
  tab->Close();
  if (tabs::TabInterface* original = tab_active_before_open_.Get()) {
    if (BrowserWindowInterface* browser =
            original->GetBrowserWindowInterface()) {
      if (auto* tab_list = TabListInterface::From(browser)) {
        tab_list->ActivateTab(tab_active_before_open_);
      }
    }
  }
  return CloseOutcome::kClosedActive;
}
```

Good:

```cpp
GeicManagedTab::CloseOutcome GeicManagedTab::Close() {
  if (!has_opened_tab_) {
    return CloseOutcome::kNotOpened;
  }
  has_opened_tab_ = false;

  tabs::TabInterface* tab = tab_.Get();
  tabs::TabHandle restore_to = tab_active_before_open_;
  tab_ = tabs::TabHandle();
  tab_active_before_open_ = tabs::TabHandle();

  if (!tab) {
    return CloseOutcome::kAlreadyClosed;
  }
  if (!IsStillOwned(tab)) {
    return CloseOutcome::kNavigatedAway;
  }

  const bool was_active = tab->IsActivated();
  tab->Close();

  if (!was_active) {
    return CloseOutcome::kClosedInactive;
  }
  if (tabs::TabInterface* original = restore_to.Get()) {
    if (BrowserWindowInterface* browser =
            original->GetBrowserWindowInterface()) {
      if (auto* tab_list = TabListInterface::From(browser)) {
        tab_list->ActivateTab(restore_to);
      }
    }
  }
  return CloseOutcome::kClosedActive;
}
```
