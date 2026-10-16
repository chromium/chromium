---
name: general/avoid-grd-strings-for-developer-ui
rubric: testing
severity: should_fix
---

**Rule.** Do not define UI string resources in .grd or .grdp files for internal
developer-only, diagnostic, or debug features. Use hardcoded string literals
directly in Java/C++ source code and suppress the Android localization linter
locally in Java using @SuppressLint("SetTextI18n").

**Why.** Defining string resources in .grd or .grdp files creates unnecessary
XML definitions, resource ID churn in R.string, and maintenance overhead for
text that is never localized or displayed to end users. For internal development
and debugging UI, hardcoding string literals and locally suppressing SetTextI18n
keeps production string catalogs clean and avoids unneeded build artifacts.

## Examples

Bad:

```cpp
<!-- Bad: Defining a non-translatable GRDP resource for an internal dev feature -->
<message name="IDS_SIDE_PANEL_DEV_TAB_TITLE" desc="Dev feature tab title placeholder" formatter_data="android_java" translateable="false">
  Tab Title: <ph name="TAB_TITLE">%s<ex>My Tab</ex></ph>
</message>

// Bad: Referencing an R.string resource for developer-only debug UI
textView.setText(tab.getContext().getString(R.string.side_panel_dev_tab_title, tab.getTitle()));
```

Good:

```cpp
// Good: Hardcoding string literal for developer-only UI and suppressing SetTextI18n lint
@SuppressLint("SetTextI18n")
private static View createTabScopedView(Tab tab) {
    ...
    textView.setText("Tab title: " + tab.getTitle() + " " + hash);
    return view;
}
```
