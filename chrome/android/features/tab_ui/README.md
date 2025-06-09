# Tab UI & Tab Management

This directory contains the majority of the code for the Tabs related UI on
Chrome Android. Notably it does not contain the tablet tab strip code which can
be found in
`//chrome/android/java/src/org/chromium/chrome/browser/compositor/overlays/strip/`.

The code in this directory builds a UI layer based upon the data layer in
`chrome/browser/tabmodel/`.

## UI Elements

The central MVC component of this directory is the `TabList*` family of classes
which binds a `TabModel` to a `RecyclerView` to present a list of tabs.

The `TabList*` MVC component is hosted by one of a few parent containers.

* `TabGroupUi*` - the bottom tab strip showing tab group information on phone
  form-factor devices.
* `TabGridDialog*` - the tab group dialog accessible from the tab switcher or
  bottom tab strip.
* `TabSwitcherPane*` - grid tab switcher or GTS, represents a `Pane` in the
  hub `chrome/browser/hub/` that host either incognito or regular tabs.
* `TabListEditor*` - represents a UI for bulk actions on tabs.
* `TabGroupList*` - notably does not use `TabList*` but rather hosts a list of
  tab groups with a different recycler view structure.

This directory also houses an assortment of UI related helper classes for
handling various operations such as picking tab group colors.