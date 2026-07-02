---
name: browser-window-feature-refactor
description: >-
  Refactor Chrome desktop `Browser`-scoped logic, such as `Browser` and
  `BrowserWindow` methods and state, into encapsulated feature controllers
  owned by `BrowserWindowFeatures`.
---

# Browser Window Feature Refactor

Use this skill for Project Bedrock refactors that modularize desktop Chrome
browser-window logic by moving ownership, methods, or initialization into
`BrowserWindowFeatures` (BWF) and its feature controllers.

## Core Constraints

1. **Classify the migration before editing.** Decide whether this is a new
   feature controller, an existing feature controller migration, or a lifecycle
   hook cleanup.
2. **Preserve behavior.** Most Bedrock refactors should have no intended
   behavior change. Keep edits narrow and avoid opportunistic cleanup.
3. **Prefer existing controllers.** If an appropriate BWF-owned controller
   already exists, move the method or state there instead of creating another
   controller.
4. **Prefer specific dependencies.** Use `BrowserWindowInterface`, `Profile`,
   `TabStripModel`, `BrowserWindow`, or feature-controller dependencies before
   reaching for `Browser*` or `GetBrowserForMigrationOnly()`.
5. **Respect BWF ordering.** Forward declarations and public accessors are
   mostly sorted. Private members and lifecycle initialization are ordered by
   ownership, lifecycle, and dependency constraints.

## Migration Types

### 1. New Feature Controller

Use this when `BrowserWindow`/`BrowserView`/`WebUIBrowserWindow` exposes
cohesive feature-specific behavior and no existing BWF-owned controller matches
it.

Workflow:

1. Create the controller near the feature's UI domain.

2. Pass only the dependencies the controller needs. Prefer
   `BrowserWindowInterface*`, `Profile*`, `TabStripModel*`, `BrowserWindow*`,
   concrete view dependencies, or other feature controller dependencies owned by
   `BrowserWindowFeatures` over `Browser*` when practical.

3. Add GN sources and deps to the narrow owning target. Verify a modular
   `BUILD.gn` exists in the feature controller's own directory and add the
   controller there, using `public`/`sources` separation (public headers in
   `public`, implementation in `sources`). Do not cargo-cult the sources into a
   monolithic target such as `//chrome/browser` or `//chrome/browser/ui`; if no
   modular target exists yet, create one in the controller's directory.

4. Own the controller with a `std::unique_ptr<FooController>` member, and in the
   same edit add a matching forward declaration `class FooController;` to the
   BWF header (see step 5 — never skip it). Initialize the member in the
   earliest correct lifecycle hook via the `BrowserWindowFeatures` user data
   factory rather than `std::make_unique`. Configuring new controllers through
   the factory keeps them easy to fake or override in tests. Add
   `Must be before/after` comments when ordering matters.

   Example:

   ```cpp
   foo_controller_ =
       GetUserDataFactory().CreateInstance<FooController>(*browser, ...);
   ```

5. **Always pair the `std::unique_ptr<FooController>` member with a forward
   declaration in the same header.** Add `class FooController;` alongside the
   other forward declarations in the BWF header — never `#include` the
   controller header (its `#include` belongs in the `.cc`). The
   `std::unique_ptr<FooController>` member from step 4 names an incomplete type,
   so omitting `class FooController;` breaks compilation. Adding the member
   without the forward declaration is the most common mistake in this migration
   — double-check the header has both before moving on.

   The header needs **two coupled edits** — the forward declaration near the top
   and the member lower down. They live far apart, so it is easy to land the
   member but forget the forward declaration. Make both edits, as shown:

   ```cpp
   // browser_window_features.h

   // Forward declarations (keep this block sorted).
   class BarController;
   class FooController;  // <-- EDIT 1: add alongside the existing declarations.
   class QuxController;

   class BrowserWindowFeatures {
     // ...
    private:
     std::unique_ptr<FooController> foo_controller_;  // <-- EDIT 2: the member.
   };
   ```

   Before moving on, open the BWF header and confirm `class FooController;` is
   present in the forward-declaration block. If it is missing, add it now — a
   `std::unique_ptr<FooController>` member with no matching forward declaration
   is a guaranteed compile failure.

6. Expose the controller through `UnownedUserData`: declare it with
   `DECLARE_USER_DATA(FooController)`, hold a
   `ui::ScopedUnownedUserData<FooController>` member, and provide a static
   `From(browser)` that returns the instance corresponding to the
   `BrowserWindowInterface`. Do **not** add a public BWF accessor; even when
   sibling controllers already expose accessors, do not mirror them for the new
   controller; add one only if an existing caller genuinely needs it.

   Example setup in `foo_controller.h`:

   ```cpp
   #include "ui/base/unowned_user_data/scoped_unowned_user_data.h"

   class FooController {
    public:
     DECLARE_USER_DATA(FooController);

     explicit FooController(BrowserWindowInterface* browser);

     // Returns the instance owned by `browser`, or nullptr.
     static FooController* From(BrowserWindowInterface* browser);

    private:
     ui::ScopedUnownedUserData<FooController> scoped_user_data_;
   };
   ```

   Matching implementation in `foo_controller.cc`:

   ```cpp
   FooController::FooController(BrowserWindowInterface* browser)
       : scoped_user_data_(browser->GetUnownedUserDataHost(), *this) {}

   // static
   FooController* FooController::From(BrowserWindowInterface* browser) {
     return Get(browser->GetUnownedUserDataHost());
   }
   ```

7. Update all callsites — in both production code and tests — to reach the
   controller through `FooController::From(browser)`. Missing callsites stay
   silent until step 8 removes the old API, then break as compilation errors in
   production and test targets; refactor them now instead of later hunting for
   the removed `BrowserWindow` methods.

8. Remove obsolete
   `BrowserWindow`/`BrowserView`/`WebUIBrowserWindow`/test-window API.

### 2. Existing Feature Controller

Use this when the target feature already has a BWF-owned controller.

Workflow:

1. Move the method, state, or initialization into the existing controller.
2. Update callsites to retrieve the existing feature through the local pattern.
3. Remove obsolete `BrowserWindow` or `BrowserView` virtual methods.
4. Keep tests pointed at the feature behavior, not the old `BrowserWindow` shim.

### 3. Lifecycle Hook Cleanup

Use this when a controller is already BWF-owned but constructed in a later hook
than its dependencies require.

Lifecycle decision tree:

- `Init()`: Feature does not require the concrete window object or view
  hierarchy. It can depend on `BrowserWindowInterface`, `Profile`,
  `TabStripModel`, session id, type, and other BWF state already created there.
- `InitPostWindowConstruction()`: Feature needs `BrowserWindow`, widget focus
  manager, `BrowserView` vs `WebUIBrowserWindow` dispatch, the view hierarchy,
  or window-level platform objects.
- `TearDownPreBrowserWindowDestruction()`: Feature has observers, raw pointers,
  view/window dependencies, or explicit teardown requirements that must be
  cleared before the window is destroyed.

## BWF Ordering Rules

Before modifying `BrowserWindowFeatures`, read
[bwf-ordering.md](references/bwf-ordering.md). Preserve the lifecycle and
ownership layout; do not mechanically alphabetize private members.

## Anti-Patterns

Avoid:

- Adding new `BrowserWindow` virtual methods or thin BWF wrappers around
  `BrowserView`.
- Creating duplicate controllers when an existing feature controller owns the
  domain.
- Adding public BWF accessors by default instead of the static controller
  accessor (`FooController::From(browser)`).
- Moving features to an earlier lifecycle hook without proving their
  dependencies exist there.

## Validation

**MANDATORY — do not report the task complete until this passes.** For a new
feature controller, re-open the BWF header and confirm it literally contains
**both** `class FooController;` (forward-declaration block) **and** the
`std::unique_ptr<FooController>` member. These are two separate edits and a tool
edit can silently fail to land; if either is missing, re-apply it and re-read
the file to verify. Treat a header missing either line as a blocking failure,
not done — a member without its forward declaration (or vice versa) does not
compile.

In the final report, state:

- the migration type;
- the selected BWF lifecycle hook and why;
- any remaining `GetBrowserForMigrationOnly()` usage and why it remains;
- for a new feature controller, that the BWF header has both
  `class FooController;` and the `std::unique_ptr<FooController>` member;
- the validation commands that passed or could not be run.
