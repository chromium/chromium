// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_ACTION_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_ACTION_API_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {

class ExtensionAction;
class ExtensionHost;

// Implementation of the action, browserAction and pageAction APIs.
//
// Divergent behaviour between the three is minimal (pageAction has required
// tabIds while browserAction's are optional, they have different internal
// browser notification requirements, and not all functions are defined for all
// APIs).
//
// Separate implementations of some functions are provided for Android vs.
// non-Android platforms. See extension_action_api_android.cc and
// extension_action_api_non_android.cc.
class ExtensionActionFunction : public ExtensionFunction {
 protected:
  ExtensionActionFunction();
  ~ExtensionActionFunction() override;
  ResponseAction Run() override;

  virtual ResponseAction RunExtensionAction() = 0;

  bool ExtractDataFromArguments();
  void NotifyChange();
  void SetVisible(bool visible);

  // All the extension action APIs take a single argument called details that
  // is a dictionary.
  raw_ptr<const base::Value::Dict> details_;

  // The tab id the extension action function should apply to, if any, or
  // kDefaultTabId if none was specified.
  int tab_id_;

  // WebContents for `tab_id_` if one exists.
  raw_ptr<content::WebContents> contents_;

  // The extension action for the current extension.
  raw_ptr<ExtensionAction> extension_action_;
};

//
// Implementations of each extension action API.
//
// pageAction and browserAction bindings are created for these by extending them
// then declaring an EXTENSION_FUNCTION_NAME.
//

// show
class ExtensionActionShowFunction : public ExtensionActionFunction {
 protected:
  ~ExtensionActionShowFunction() override = default;
  ResponseAction RunExtensionAction() override;
};

// hide
class ExtensionActionHideFunction : public ExtensionActionFunction {
 protected:
  ~ExtensionActionHideFunction() override = default;
  ResponseAction RunExtensionAction() override;
};

// setIcon
class ExtensionActionSetIconFunction : public ExtensionActionFunction {
 public:
  static void SetReportErrorForInvisibleIconForTesting(bool value);

 protected:
  ~ExtensionActionSetIconFunction() override = default;
  ResponseAction RunExtensionAction() override;
};

// setTitle
class ExtensionActionSetTitleFunction : public ExtensionActionFunction {
 protected:
  ~ExtensionActionSetTitleFunction() override = default;
  ResponseAction RunExtensionAction() override;
};

// setPopup
class ExtensionActionSetPopupFunction : public ExtensionActionFunction {
 protected:
  ~ExtensionActionSetPopupFunction() override = default;
  ResponseAction RunExtensionAction() override;
};

// setBadgeText
class ExtensionActionSetBadgeTextFunction : public ExtensionActionFunction {
 protected:
  ~ExtensionActionSetBadgeTextFunction() override = default;
  ResponseAction RunExtensionAction() override;
};

// setBadgeBackgroundColor
class ExtensionActionSetBadgeBackgroundColorFunction
    : public ExtensionActionFunction {
 protected:
  ~ExtensionActionSetBadgeBackgroundColorFunction() override = default;
  ResponseAction RunExtensionAction() override;
};

// getTitle
class ExtensionActionGetTitleFunction : public ExtensionActionFunction {
 protected:
  ~ExtensionActionGetTitleFunction() override = default;
  ResponseAction RunExtensionAction() override;
};

// getPopup
class ExtensionActionGetPopupFunction : public ExtensionActionFunction {
 protected:
  ~ExtensionActionGetPopupFunction() override = default;
  ResponseAction RunExtensionAction() override;
};

// getBadgeText
class ExtensionActionGetBadgeTextFunction : public ExtensionActionFunction {
 protected:
  ~ExtensionActionGetBadgeTextFunction() override = default;
  ResponseAction RunExtensionAction() override;
};

// getBadgeBackgroundColor
class ExtensionActionGetBadgeBackgroundColorFunction
    : public ExtensionActionFunction {
 protected:
  ~ExtensionActionGetBadgeBackgroundColorFunction() override = default;
  ResponseAction RunExtensionAction() override;
};

//
// action.* aliases for supported action APIs.
//

class ActionSetIconFunction : public ExtensionActionSetIconFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.setIcon", ACTION_SETICON)

 protected:
  ~ActionSetIconFunction() override = default;
};

class ActionGetPopupFunction : public ExtensionActionGetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.getPopup", ACTION_GETPOPUP)

 protected:
  ~ActionGetPopupFunction() override = default;
};

class ActionSetPopupFunction : public ExtensionActionSetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.setPopup", ACTION_SETPOPUP)

 protected:
  ~ActionSetPopupFunction() override = default;
};

class ActionGetTitleFunction : public ExtensionActionGetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.getTitle", ACTION_GETTITLE)

 protected:
  ~ActionGetTitleFunction() override = default;
};

class ActionSetTitleFunction : public ExtensionActionSetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.setTitle", ACTION_SETTITLE)

 protected:
  ~ActionSetTitleFunction() override = default;
};

class ActionGetBadgeTextFunction : public ExtensionActionGetBadgeTextFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.getBadgeText", ACTION_GETBADGETEXT)

 protected:
  ~ActionGetBadgeTextFunction() override = default;
};

class ActionSetBadgeTextFunction : public ExtensionActionSetBadgeTextFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.setBadgeText", ACTION_SETBADGETEXT)

 protected:
  ~ActionSetBadgeTextFunction() override = default;
};

class ActionGetBadgeBackgroundColorFunction
    : public ExtensionActionGetBadgeBackgroundColorFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.getBadgeBackgroundColor",
                             ACTION_GETBADGEBACKGROUNDCOLOR)

 protected:
  ~ActionGetBadgeBackgroundColorFunction() override = default;
};

class ActionSetBadgeBackgroundColorFunction
    : public ExtensionActionSetBadgeBackgroundColorFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.setBadgeBackgroundColor",
                             ACTION_SETBADGEBACKGROUNDCOLOR)

 protected:
  ~ActionSetBadgeBackgroundColorFunction() override = default;
};

class ActionGetBadgeTextColorFunction : public ExtensionActionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.getBadgeTextColor",
                             ACTION_GETBADGETEXTCOLOR)

 protected:
  ~ActionGetBadgeTextColorFunction() override = default;
  ResponseAction RunExtensionAction() override;
};

class ActionSetBadgeTextColorFunction : public ExtensionActionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.setBadgeTextColor",
                             ACTION_SETBADGETEXTCOLOR)

 protected:
  ~ActionSetBadgeTextColorFunction() override = default;
  ResponseAction RunExtensionAction() override;
};

class ActionEnableFunction : public ExtensionActionShowFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.enable", ACTION_ENABLE)

 protected:
  ~ActionEnableFunction() override = default;
};

class ActionDisableFunction : public ExtensionActionHideFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.disable", ACTION_DISABLE)

 protected:
  ~ActionDisableFunction() override = default;
};

class ActionIsEnabledFunction : public ExtensionActionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.isEnabled", ACTION_ISENABLED)

 protected:
  ~ActionIsEnabledFunction() override = default;
  ResponseAction RunExtensionAction() override;
};

class ActionGetUserSettingsFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.getUserSettings", ACTION_GETUSERSETTINGS)

  ActionGetUserSettingsFunction();
  ActionGetUserSettingsFunction(const ActionGetUserSettingsFunction&) = delete;
  ActionGetUserSettingsFunction& operator=(
      const ActionGetUserSettingsFunction&) = delete;

  ResponseAction Run() override;

 protected:
  ~ActionGetUserSettingsFunction() override;
};

// Note: action.openPopup() and browserAction.openPopup() have subtly different
// implementations:
//   * action.openPopup() allows the extension to specify a window ID.
//   * browserAction.openPopup() will time out after 10 seconds;
//     action.openPopup() does not time out and instead waits for the popup to
//     either be shown or encounter an error.
//   * browserAction.openPopup() returns a handle to the HTMLWindow of the
//     popup; action.openPopup() returns nothing.
// Due to these differences, the implementations are distinct classes.
class ActionOpenPopupFunction : public ExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.openPopup", ACTION_OPENPOPUP)

  ActionOpenPopupFunction();
  ActionOpenPopupFunction(const ActionOpenPopupFunction&) = delete;
  ActionOpenPopupFunction& operator=(const ActionOpenPopupFunction&) = delete;

 protected:
  // ExtensionFunction:
  ~ActionOpenPopupFunction() override;
  ResponseAction Run() override;

  void OnShowPopupComplete(ExtensionHost* popup_host);
};

//
// browserAction.* aliases for supported browserAction APIs.
//

class BrowserActionSetIconFunction : public ExtensionActionSetIconFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setIcon", BROWSERACTION_SETICON)

 protected:
  ~BrowserActionSetIconFunction() override = default;
};

class BrowserActionSetTitleFunction : public ExtensionActionSetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setTitle", BROWSERACTION_SETTITLE)

 protected:
  ~BrowserActionSetTitleFunction() override = default;
};

class BrowserActionSetPopupFunction : public ExtensionActionSetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setPopup", BROWSERACTION_SETPOPUP)

 protected:
  ~BrowserActionSetPopupFunction() override = default;
};

class BrowserActionGetTitleFunction : public ExtensionActionGetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getTitle", BROWSERACTION_GETTITLE)

 protected:
  ~BrowserActionGetTitleFunction() override = default;
};

class BrowserActionGetPopupFunction : public ExtensionActionGetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getPopup", BROWSERACTION_GETPOPUP)

 protected:
  ~BrowserActionGetPopupFunction() override = default;
};

class BrowserActionSetBadgeTextFunction
    : public ExtensionActionSetBadgeTextFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setBadgeText",
                             BROWSERACTION_SETBADGETEXT)

 protected:
  ~BrowserActionSetBadgeTextFunction() override = default;
};

class BrowserActionSetBadgeBackgroundColorFunction
    : public ExtensionActionSetBadgeBackgroundColorFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setBadgeBackgroundColor",
                             BROWSERACTION_SETBADGEBACKGROUNDCOLOR)

 protected:
  ~BrowserActionSetBadgeBackgroundColorFunction() override = default;
};

class BrowserActionGetBadgeTextFunction
    : public ExtensionActionGetBadgeTextFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getBadgeText",
                             BROWSERACTION_GETBADGETEXT)

 protected:
  ~BrowserActionGetBadgeTextFunction() override = default;
};

class BrowserActionGetBadgeBackgroundColorFunction
    : public ExtensionActionGetBadgeBackgroundColorFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getBadgeBackgroundColor",
                             BROWSERACTION_GETBADGEBACKGROUNDCOLOR)

 protected:
  ~BrowserActionGetBadgeBackgroundColorFunction() override = default;
};

class BrowserActionEnableFunction : public ExtensionActionShowFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.enable", BROWSERACTION_ENABLE)

 protected:
  ~BrowserActionEnableFunction() override = default;
};

class BrowserActionDisableFunction : public ExtensionActionHideFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.disable", BROWSERACTION_DISABLE)

 protected:
  ~BrowserActionDisableFunction() override = default;
};

// Note: action.openPopup() and browserAction.openPopup() have subtly different
// implementations. See ActionOpenPopupFunction above.
// TODO(devlin): Remove browserAction.openPopup().
class BrowserActionOpenPopupFunction : public ExtensionFunction,
                                       public ExtensionHostRegistry::Observer {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.openPopup",
                             BROWSERACTION_OPEN_POPUP)
  BrowserActionOpenPopupFunction();

  BrowserActionOpenPopupFunction(const BrowserActionOpenPopupFunction&) =
      delete;
  BrowserActionOpenPopupFunction& operator=(
      const BrowserActionOpenPopupFunction&) = delete;

 private:
  ~BrowserActionOpenPopupFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
  void OnBrowserContextShutdown() override;

  // ExtensionHostRegistry::Observer:
  void OnExtensionHostCompletedFirstLoad(
      content::BrowserContext* browser_context,
      ExtensionHost* host) override;

  void OpenPopupTimedOut();

  base::ScopedObservation<ExtensionHostRegistry,
                          ExtensionHostRegistry::Observer>
      host_registry_observation_{this};
};

}  // namespace extensions

//
// pageAction.* aliases for supported pageAction APIs.
//

class PageActionShowFunction : public extensions::ExtensionActionShowFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.show", PAGEACTION_SHOW)

 protected:
  ~PageActionShowFunction() override = default;
};

class PageActionHideFunction : public extensions::ExtensionActionHideFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.hide", PAGEACTION_HIDE)

 protected:
  ~PageActionHideFunction() override = default;
};

class PageActionSetIconFunction
    : public extensions::ExtensionActionSetIconFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.setIcon", PAGEACTION_SETICON)

 protected:
  ~PageActionSetIconFunction() override = default;
};

class PageActionSetTitleFunction
    : public extensions::ExtensionActionSetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.setTitle", PAGEACTION_SETTITLE)

 protected:
  ~PageActionSetTitleFunction() override = default;
};

class PageActionSetPopupFunction
    : public extensions::ExtensionActionSetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.setPopup", PAGEACTION_SETPOPUP)

 protected:
  ~PageActionSetPopupFunction() override = default;
};

class PageActionGetTitleFunction
    : public extensions::ExtensionActionGetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.getTitle", PAGEACTION_GETTITLE)

 protected:
  ~PageActionGetTitleFunction() override = default;
};

class PageActionGetPopupFunction
    : public extensions::ExtensionActionGetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.getPopup", PAGEACTION_GETPOPUP)

 protected:
  ~PageActionGetPopupFunction() override = default;
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_ACTION_API_H_
