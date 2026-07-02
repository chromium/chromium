# BrowserWindowFeatures Ordering

`BrowserWindowFeatures` ordering is intentionally structured. Preserve the
current layout when adding or moving feature ownership.

## Public Header

In `browser_window_features.h`:

- Forward-declare feature controller and dependency types in the header; do not
  `#include` their headers here. `std::unique_ptr<T>` and `raw_ptr<T>` members
  only need a forward declaration of `T`, so add `class FooController;` to the
  forward-declaration block and put the controller's `#include` in
  `browser_window_features.cc` instead. This keeps the aggregator header
  dependency-light. When you add a new BWF-owned controller, follow the existing
  pattern in the file rather than including the new controller's header.
- Keep top-level forward declarations sorted alphabetically within their
  existing unconditional group.
- Keep platform-conditional forward declarations sorted within each buildflag
  group.
- Keep namespaced forward declarations sorted by namespace and class within the
  existing namespace sections.
- Keep public inline accessors sorted by accessor name unless an existing local
  comment explains a special case.
- Do not add a public accessor by default. Prefer `UnownedUserData` for new
  feature access.

## Private Members

Do not mechanically alphabetize private members. Declaration order controls
destruction order, so changes require dependency review.

Private members are grouped by ownership:

1. members owned by all browser window types;
2. members owned only when a `BrowserView` is attached;
3. members owned only when a `WebUIBrowserWindow` is used;
4. platform-specific members;
5. non-owning references;
6. embedder features last.

Within a group, keep related dependency clusters together. If a member must be
out of alphabetical position, add or preserve a `Must be before` or
`Must be after` comment.

New BWF-owned controllers must make their dependencies explicit when ordering
matters. Construct them through
`GetUserDataFactory().CreateInstance<FooController>(*browser, ...)` (which
returns a `std::unique_ptr<>`), not `std::make_unique`. Put the dependency
comment next to the member declaration or the initialization site, following the
local style:

```cpp
// Must be after session_service_browser_helper_:
//   tab_list_bridge_ depends on initialized session tab/window state.
tab_list_bridge_ =
    GetUserDataFactory().CreateInstance<TabListBridge>(*browser, ...);
```

For a new `FooWindowController`, call out both directions when relevant:

- what `foo_window_controller_` depends on;
- what later BWF feature depends on `foo_window_controller_` being initialized.

## Lifecycle Methods

The three lifecycle methods follow a consistent layout:

1. A short prelude captures shared locals and foundational state.
2. Members owned by all browser window types.
3. Members owned only by `TYPE_NORMAL` browser windows.
4. Embedder features are initialized last.

Dependency ordering takes precedence over alphabetical ordering. Important
ordering constraints should be called out inline.

## Teardown

`TearDownPreBrowserWindowDestruction()` tears down in reverse order of the
construction lifecycle:

1. embedder features;
2. `InitPostWindowConstruction()` members;
3. `Init()` members.

Within each section, reset members in reverse of the corresponding
initialization order unless a dependency comment explains a different order.
