# Desktop InfoBars (`BrowserInfoBarManager`)

This directory contains the centralized Desktop InfoBar framework and utilities
for working with [infobars](/components/infobars/README.md) in `chrome/browser`.

## IMPORTANT: How to Create or Migrate a Desktop InfoBar

**DO NOT** create new subclasses of `ConfirmInfoBarDelegate` or
`SimpleAlertInfoBarDelegate` for Desktop Chrome, and **DO NOT** call
`CreateConfirmInfoBar()`, `CreateSimpleAlertInfoBar()`, or
`ConfirmInfoBar::Create()` directly.

All new and migrated Desktop infobars **must** use the centralized declarative
framework via `infobars::InfoBarSpec` and `infobars::BrowserInfoBarManager`.

---

## Core Architecture

* **`infobars::InfoBarSpec` & `InfoBarSpec::Builder`**
  ([`infobar_spec.h`](infobar_spec.h)): Declaratively defines an infobar's
  appearance and behavior, including its `InfoBarIdentifier`, message text or
  template (`SetMessageText()` / `SetMessageTextTemplate()`), icon
  (`SetIcon()`), buttons (`AddOkButton()` / `AddCancelButton()`), links,
  priority (`SetPriority()`), and lifecycle scope (`InfoBarScope::kTab` or
  `InfoBarScope::kGlobal`).
* **`infobars::BrowserInfoBarManager`**
  ([`browser_infobar_manager.h`](browser_infobar_manager.h)): A browser-process
  global feature (`BrowserInfoBarManager::From(g_browser_process)`) that stores
  registered `InfoBarSpec` definitions and manages showing, hiding, active-tab
  mirroring (for global infobars), and UMA metrics.
* **`infobars::InfoBarShowParams`** ([`infobar_spec.h`](infobar_spec.h)):
  Optional per-show overrides for values only known when the infobar is
  triggered (e.g., dynamic `message_text`, template `substitutions`, or
  instance-specific button/link/result callbacks).
* **Central Registry**
  ([`//chrome/browser/ui/infobars/browser_infobar_registry.cc`](/chrome/browser/ui/infobars/browser_infobar_registry.cc)):
  Where `InfoBarSpec` instances are built and registered on startup via
  `RegisterInfoBars()` (or `RegisterPreProfileInitInfoBars()` for early startup
  infobars).

---

## Step-by-Step Guide for Creating a New Desktop InfoBar

> **Note for New InfoBars vs. Migrations:**
> If you are creating a **new** infobar with the centralized framework, a
> migration study / feature flag (`IsInfoBarMigrated()`, `infobar_features.h`,
> or `fieldtrial_testing_config.json`) is **not** needed. Register the spec
> unconditionally in `browser_infobar_registry.cc` and call
> `BrowserInfoBarManager::Show()` directly. (`IsInfoBarMigrated()` is only used
> when migrating pre-existing legacy infobars that retain a legacy fallback).

### 1. Define the Identifier
Add a new identifier to `infobars::InfoBarDelegate::InfoBarIdentifier` in
[`//components/infobars/core/infobar_delegate.h`](/components/infobars/core/infobar_delegate.h)
and add the corresponding entry to the `InfoBarIdentifier` enum in
[`//tools/metrics/histograms/metadata/browser/enums.xml`](/tools/metrics/histograms/metadata/browser/enums.xml).

### 2. Register the `InfoBarSpec`
In [`//chrome/browser/ui/infobars/browser_infobar_registry.cc`](/chrome/browser/ui/infobars/browser_infobar_registry.cc),
register your infobar specification inside `RegisterInfoBars()` (or
`RegisterPreProfileInitInfoBars()` if shown before profile initialization):

```cpp
auto spec =
    InfoBarSpec::Builder(InfoBarDelegate::MY_FEATURE_INFOBAR_DELEGATE)
        .SetMessageText(l10n_util::GetStringUTF16(IDS_MY_FEATURE_INFOBAR_TEXT))
        .SetIcon(vector_icons::kInfoOutlineIcon)
        .SetScope(InfoBarScope::kTab)
        .AddOkButton(
            l10n_util::GetStringUTF16(IDS_MY_FEATURE_OK_BUTTON),
            base::BindRepeating([](content::WebContents* web_contents) {
              // Handle acceptance.
            }))
        .Build();
browser_infobar_manager->Register(std::move(spec));
```

### 3. Show (and Hide) the InfoBar
In your feature's trigger call site (e.g., the feature controller, navigation
observer, tab helper, or startup flow where the infobar should be displayed or
dismissed), call `BrowserInfoBarManager` instead of creating a delegate or
calling `ContentInfoBarManager::AddInfoBar()`:

* **Tab-Scoped InfoBar (`InfoBarScope::kTab`):**
  ```cpp
  auto* manager = infobars::BrowserInfoBarManager::From(g_browser_process);
  if (tabs::TabInterface* tab =
          tabs::TabInterface::MaybeGetFromContents(web_contents)) {
    manager->Show(tab, infobars::InfoBarDelegate::MY_FEATURE_INFOBAR_DELEGATE);
  }
  ```
  If runtime values (such as dynamic strings, template substitutions, or state
  bound to a callback) are needed, pass `infobars::InfoBarShowParams`:
  ```cpp
  infobars::InfoBarShowParams params;
  params.message_text = dynamic_message;
  manager->Show(tab, infobars::InfoBarDelegate::MY_FEATURE_INFOBAR_DELEGATE,
                std::move(params));
  ```

* **Global InfoBar (`InfoBarScope::kGlobal`):**
  ```cpp
  auto* manager = infobars::BrowserInfoBarManager::From(g_browser_process);
  manager->ShowGlobally(
      infobars::InfoBarDelegate::MY_FEATURE_INFOBAR_DELEGATE);
  ```

* **Hiding an InfoBar Programmatically:**
  ```cpp
  // For a tab-scoped InfoBar:
  manager->Hide(web_contents,
                infobars::InfoBarDelegate::MY_FEATURE_INFOBAR_DELEGATE);

  // For a global InfoBar:
  manager->Hide(infobars::InfoBarDelegate::MY_FEATURE_INFOBAR_DELEGATE);
  ```

---

## Legacy Utilities (Do Not Use in New Code)

* `confirm_infobar_creator.h` (`CreateConfirmInfoBar`) and
  `simple_alert_infobar_creator.h` (`CreateSimpleAlertInfoBar`) are legacy
  helpers retained only for unmigrated infobars and internal use by
  `BrowserInfoBarManager`. Do not add new callers.

