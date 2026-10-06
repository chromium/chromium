# Desktop InfoBar Guidelines

These instructions apply to Desktop Chrome InfoBars implemented through
`chrome/browser/infobars/`.

## Use the Centralized Framework
<!-- TODO(https://crbug.com/567121669): Update this file once all desktop
infobar migrations complete and the base::PassKey compile-time gate lands on
CreateConfirmInfoBar(), CreateSimpleAlertInfoBar(), and
ConfirmInfoBar::Create(). -->

For new Desktop InfoBars, use:

- `infobars::InfoBarSpec` to define appearance and behavior.
- `infobars::BrowserInfoBarManager` to register, show, and hide instances.
- `infobars::InfoBarShowParams` for values known only when showing an InfoBar.

Do not introduce new feature-specific subclasses of
`ConfirmInfoBarDelegate` or `SimpleAlertInfoBarDelegate`.

Do not create feature InfoBars by calling `CreateConfirmInfoBar()`,
`CreateSimpleAlertInfoBar()`, `ConfirmInfoBar::Create()`, or
`ContentInfoBarManager::AddInfoBar()` directly.

These restrictions concern feature implementation. They do not prohibit
maintaining the centralized framework's internal creation code or the
legacy implementation of an InfoBar that has not yet been migrated.

Before changing an InfoBar, inspect its registration, trigger call sites,
and the current API contracts in:

- [infobar_spec.h](infobar_spec.h)
- [browser_infobar_manager.h](browser_infobar_manager.h)
- [browser_infobar_registry.cc](/chrome/browser/ui/infobars/browser_infobar_registry.cc)

## Distinguish New InfoBars from Migrations

### New InfoBars

A new InfoBar using the centralized framework does not need a migration
study or a legacy fallback.

- Do not add a migration parameter to `infobar_features.h` or `.cc`.
- Do not guard its registration or display with `IsInfoBarMigrated()`.
- Do not add a migration study to `fieldtrial_testing_config.json`.
- Register its specification in the appropriate startup registration
  function.
- Show it through `BrowserInfoBarManager`.

"Unconditional registration" means registration without a migration gate.
Keep required platform and build guards.

A feature may still need its own product rollout or eligibility checks.
Those checks are separate from migration machinery; this guidance does
not require removing them.

### Existing InfoBars Being Migrated

An existing legacy InfoBar may temporarily need `IsInfoBarMigrated()` and
a legacy fallback.

When migrating:

- Follow the existing migration conventions.
- Ensure registration and display use compatible conditions.
- Preserve eligibility, actions, dismissal, navigation, and metrics
  behavior unless the change explicitly intends to modify them.
- Ensure one trigger cannot show both the legacy and centralized versions.
- Do not remove fallback paths or rollout controls as an incidental change.

## Creating a New InfoBar

### 1. Add the Identifier and Metrics Metadata

Append a new identifier to `InfoBarDelegate::InfoBarIdentifier` in
[infobar_delegate.h](/components/infobars/core/infobar_delegate.h).

- Use a fresh numeric value.
- Do not renumber existing values or reuse removed values.
- Add the matching value and label to the `InfoBarIdentifier` enum in
  [metrics enums.xml](/tools/metrics/histograms/metadata/browser/enums.xml).
- Follow the enum's `IfChange` / `ThenChange` requirements.

### 2. Register the Specification

Register the `InfoBarSpec` in
[browser_infobar_registry.cc](/chrome/browser/ui/infobars/browser_infobar_registry.cc).

Use `RegisterInfoBars()` for normal registration, or
`RegisterPreProfileInitInfoBars()` when the InfoBar must be available
before profile initialization.

Register each identifier exactly once. Do not register it in both
functions.

Example within the registry's `infobars` namespace:

```cpp
auto spec =
    InfoBarSpec::Builder(InfoBarDelegate::MY_FEATURE_INFOBAR_DELEGATE)
        .SetMessageText(
            l10n_util::GetStringUTF16(IDS_MY_FEATURE_INFOBAR_TEXT))
        .SetScope(InfoBarScope::kTab)
        .AddOkButton(
            l10n_util::GetStringUTF16(IDS_MY_FEATURE_OK_BUTTON),
            base::BindRepeating([](content::WebContents* web_contents) {
              // Perform the feature action.
              // Follow the ActionCallback lifetime contract.
            }))
        .Build();

browser_infobar_manager->Register(std::move(spec));
```

Use localized resources for user-visible text. Configure the icon,
priority, buttons, links, and lifecycle behavior as appropriate.

### 3. Choose Scope and Lifecycle Deliberately

Use `InfoBarScope::kTab` when the InfoBar belongs to a particular tab.

Use `InfoBarScope::kGlobal` when the InfoBar should be managed across
browser windows, including active-tab mirroring.

For global InfoBars, determine which browser windows are eligible.
Use the framework's browser filter when the feature requires restrictions.

Inspect the current defaults in `infobar_spec.h`. Explicitly configure
behavior when the feature requires different handling for:

- Navigation expiration.
- Fullscreen visibility.
- User dismissal.
- Closing after acceptance.
- Priority.

Do not assume that changing scope alone preserves the lifecycle behavior
of a legacy InfoBar.

### 4. Pass Runtime State at Show Time

Keep registration suitable for a browser-process-wide specification.

Use `InfoBarShowParams` for dynamic message text, template substitutions,
or per-instance callbacks. Avoid binding tab- or profile-specific objects
into a process-wide registered callback without a valid lifetime strategy.

For example:

```cpp
auto* manager = infobars::BrowserInfoBarManager::From(g_browser_process);

if (tabs::TabInterface* tab =
        tabs::TabInterface::MaybeGetFromContents(web_contents)) {
  infobars::InfoBarShowParams params;
  params.message_text = dynamic_message;

  manager->Show(
      tab,
      infobars::InfoBarDelegate::MY_FEATURE_INFOBAR_DELEGATE,
      std::move(params));
}
```

This example assumes the manager has been initialized and the identifier
has been registered. Verify those prerequisites for the actual call site,
including its test environment.

### 5. Show Global InfoBars Through the Global API

For a specification registered with `InfoBarScope::kGlobal`:

```cpp
auto* manager = infobars::BrowserInfoBarManager::From(g_browser_process);

manager->ShowGlobally(
    infobars::InfoBarDelegate::MY_FEATURE_INFOBAR_DELEGATE);
```

Global per-show parameters also apply to mirrored instances created later.
Ensure callbacks and captured state remain valid for that lifetime.

### 6. Use the Correct Dismissal Overload

For a tab-scoped InfoBar:

```cpp
manager->Hide(
    web_contents,
    infobars::InfoBarDelegate::MY_FEATURE_INFOBAR_DELEGATE);
```

For a global InfoBar:

```cpp
manager->Hide(
    infobars::InfoBarDelegate::MY_FEATURE_INFOBAR_DELEGATE);
```

The `Hide(web_contents, identifier)` overload skips globally tracked
instances. It must not be used to dismiss a global InfoBar.

The identifier-only overload has different behavior for tab-scoped
InfoBars: it targets the last-active browser's active tab. Prefer the
`WebContents` overload when dismissing a particular tab's InfoBar.

## Callback and Ownership Rules

Follow the callback contracts documented in `infobar_spec.h`.

- Action callbacks must not synchronously destroy the InfoBar or close
  its tab. Use the documented teardown-safe mechanism or a result
  callback when the action requires teardown first.
- A result callback may receive a null `WebContents`; handle that case.
- Programmatic removal does not report a terminal result. Do not make
  essential cleanup depend exclusively on a result callback.
- Use an appropriate lifetime strategy for captured objects. Do not
  capture short-lived objects by reference in long-lived callbacks.
- If retaining an InfoBar returned by `Show()`, use `InfoBar::AsWeakPtr()`.
  The InfoBar may close and be destroyed at any time.

## Verification Expectations

For executable changes, add or update focused tests for the behavior
affected by the change.

Relevant scenarios may include:

- Registration before the first display attempt.
- Display in an eligible tab or browser.
- Button actions and user dismissal.
- Programmatic dismissal using the correct scope.
- Navigation or tab closure.
- Global mirroring and browser eligibility.
- Callback lifetime and cleanup.
- Both migration paths while a legacy fallback remains.

Choose tests based on the change; do not add unrelated coverage merely to
satisfy this list.

For documentation-only changes, verify examples against the current
headers and implementations. Keep this file and the
[README](README.md) consistent.
