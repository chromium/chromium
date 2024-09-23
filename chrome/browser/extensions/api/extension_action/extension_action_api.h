// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_ACTION_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_ACTION_API_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/ui/extensions/extension_popup_types.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_action.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_host_registry.h"
#include "extensions/common/extension_id.h"
#include "third_party/skia/include/core/SkColor.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {

class ExtensionHost;
class ExtensionPrefs;

class ExtensionActionAPI : public BrowserContextKeyedAPI {
 public:
  class Observer {
   public:
    // Called when there is a change to the given |extension_action|.
    // |web_contents| is the web contents that was affected, and
    // |browser_context| is the associated BrowserContext. (The latter is
    // included because ExtensionActionAPI is shared between normal and
    // incognito contexts, so |browser_context| may not equal
    // |browser_context_|.)
    virtual void OnExtensionActionUpdated(
        ExtensionAction* extension_action,
        content::WebContents* web_contents,
        content::BrowserContext* browser_context);

    // Called when the ExtensionActionAPI is shutting down, giving observers a
    // chance to unregister themselves if there is not a definitive lifecycle.
    virtual void OnExtensionActionAPIShuttingDown();

   protected:
    virtual ~Observer();
  };

  explicit ExtensionActionAPI(content::BrowserContext* context);

  ExtensionActionAPI(const ExtensionActionAPI&) = delete;
  ExtensionActionAPI& operator=(const ExtensionActionAPI&) = delete;

  ~ExtensionActionAPI() override;

  // Convenience method to get the instance for a profile.
  static ExtensionActionAPI* Get(content::BrowserContext* context);

  static BrowserContextKeyedAPIFactory<ExtensionActionAPI>*
      GetFactoryInstance();

  // Add or remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notifies that there has been a change in the given |extension_action|.
  void NotifyChange(ExtensionAction* extension_action,
                    content::WebContents* web_contents,
                    content::BrowserContext* browser_context);

  // Dispatches the onClicked event for extension that owns the given action.
  void DispatchExtensionActionClicked(const ExtensionAction& extension_action,
                                      content::WebContents* web_contents,
                                      const Extension* extension);

  // Called when the action for the given extension is pinned or unpinned from
  // the toolbar. Dispatches the onUserSettingsChanged event for extension that
  // owns the given action.
  void OnActionPinnedStateChanged(const ExtensionId& extension_id,
                                  bool is_pinned);

  // Clears the values for all ExtensionActions for the tab associated with the
  // given |web_contents| (and signals that page actions changed).
  void ClearAllValuesForTab(content::WebContents* web_contents);

  void set_prefs_for_testing(ExtensionPrefs* prefs) {
    extension_prefs_ = prefs;
  }

 private:
  friend class BrowserContextKeyedAPIFactory<ExtensionActionAPI>;

  // Returns the associated extension prefs.
  ExtensionPrefs* GetExtensionPrefs();

  // The DispatchEvent methods forward events to the |context|'s event router.
  void DispatchEventToExtension(content::BrowserContext* context,
                                const ExtensionId& extension_id,
                                events::HistogramValue histogram_value,
                                const std::string& event_name,
                                base::Value::List event_args);

  // BrowserContextKeyedAPI implementation.
  void Shutdown() override;
  static const char* service_name() { return "ExtensionActionAPI"; }
  static const bool kServiceRedirectedInIncognito = true;

  base::ObserverList<Observer>::Unchecked observers_;

  raw_ptr<content::BrowserContext> browser_context_;

  raw_ptr<ExtensionPrefs> extension_prefs_;
};

// Implementation of the browserAction and pageAction APIs.
//
// Divergent behaviour between the two is minimal (pageAction has required
// tabIds while browserAction's are optional, they have different internal
// browser notification requirements, and not all functions are defined for all
// APIs).
class ExtensionActionFunction : public ExtensionFunction {
 public:
  static bool ParseCSSColorString(const std::string& color_string,
                                  SkColor* result);

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
  raw_ptr<base::Value::Dict> details_;

  // The tab id the extension action function should apply to, if any, or
  // kDefaultTabId if none was specified.
  int tab_id_;

  // WebContents for |tab_id_| if one exists.
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
  ~ExtensionActionShowFunction() override {}
  ResponseAction RunExtensionAction() override;
};

// hide
class ExtensionActionHideFunction : public ExtensionActionFunction {
 protected:
  ~ExtensionActionHideFunction() override {}
  ResponseAction RunExtensionAction() override;
};

// setIcon
class ExtensionActionSetIconFunction : public ExtensionActionFunction {
 public:
  static void SetReportErrorForInvisibleIconForTesting(bool value);

 protected:
  ~ExtensionActionSetIconFunction() override {}
  ResponseAction RunExtensionAction() override;
};

// setTitle
class ExtensionActionSetTitleFunction : public ExtensionActionFunction {
 protected:
  ~ExtensionActionSetTitleFunction() override {}
  ResponseAction RunExtensionAction() override;
};

// setPopup
class ExtensionActionSetPopupFunction : public ExtensionActionFunction {
 protected:
  ~ExtensionActionSetPopupFunction() override {}
  ResponseAction RunExtensionAction() override;
};

// setBadgeText
class ExtensionActionSetBadgeTextFunction : public ExtensionActionFunction {
 protected:
  ~ExtensionActionSetBadgeTextFunction() override {}
  ResponseAction RunExtensionAction() override;
};

// setBadgeBackgroundColor
class ExtensionActionSetBadgeBackgroundColorFunction
    : public ExtensionActionFunction {
 protected:
  ~ExtensionActionSetBadgeBackgroundColorFunction() override {}
  ResponseAction RunExtensionAction() override;
};

// getTitle
class ExtensionActionGetTitleFunction : public ExtensionActionFunction {
 protected:
  ~ExtensionActionGetTitleFunction() override {}
  ResponseAction RunExtensionAction() override;
};

// getPopup
class ExtensionActionGetPopupFunction : public ExtensionActionFunction {
 protected:
  ~ExtensionActionGetPopupFunction() override {}
  ResponseAction RunExtensionAction() override;
};

// getBadgeText
class ExtensionActionGetBadgeTextFunction : public ExtensionActionFunction {
 protected:
  ~ExtensionActionGetBadgeTextFunction() override {}
  ResponseAction RunExtensionAction() override;
};

// getBadgeBackgroundColor
class ExtensionActionGetBadgeBackgroundColorFunction
    : public ExtensionActionFunction {
 protected:
  ~ExtensionActionGetBadgeBackgroundColorFunction() override {}
  ResponseAction RunExtensionAction() override;
};

//
// action.* aliases for supported action APIs.
//

class ActionSetIconFunction : public ExtensionActionSetIconFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.setIcon", ACTION_SETICON)

 protected:
  ~ActionSetIconFunction() override {}
};

class ActionGetPopupFunction : public ExtensionActionGetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.getPopup", ACTION_GETPOPUP)

 protected:
  ~ActionGetPopupFunction() override {}
};

class ActionSetPopupFunction : public ExtensionActionSetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.setPopup", ACTION_SETPOPUP)

 protected:
  ~ActionSetPopupFunction() override {}
};

class ActionGetTitleFunction : public ExtensionActionGetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.getTitle", ACTION_GETTITLE)

 protected:
  ~ActionGetTitleFunction() override {}
};

class ActionSetTitleFunction : public ExtensionActionSetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.setTitle", ACTION_SETTITLE)

 protected:
  ~ActionSetTitleFunction() override {}
};

class ActionGetBadgeTextFunction : public ExtensionActionGetBadgeTextFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.getBadgeText", ACTION_GETBADGETEXT)

 protected:
  ~ActionGetBadgeTextFunction() override {}
};

class ActionSetBadgeTextFunction : public ExtensionActionSetBadgeTextFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.setBadgeText", ACTION_SETBADGETEXT)

 protected:
  ~ActionSetBadgeTextFunction() override {}
};

class ActionGetBadgeBackgroundColorFunction
    : public ExtensionActionGetBadgeBackgroundColorFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.getBadgeBackgroundColor",
                             ACTION_GETBADGEBACKGROUNDCOLOR)

 protected:
  ~ActionGetBadgeBackgroundColorFunction() override {}
};

class ActionSetBadgeBackgroundColorFunction
    : public ExtensionActionSetBadgeBackgroundColorFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.setBadgeBackgroundColor",
                             ACTION_SETBADGEBACKGROUNDCOLOR)

 protected:
  ~ActionSetBadgeBackgroundColorFunction() override {}
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
  ~ActionEnableFunction() override {}
};

class ActionDisableFunction : public ExtensionActionHideFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("action.disable", ACTION_DISABLE)

 protected:
  ~ActionDisableFunction() override {}
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
  ~BrowserActionSetIconFunction() override {}
};

class BrowserActionSetTitleFunction : public ExtensionActionSetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setTitle", BROWSERACTION_SETTITLE)

 protected:
  ~BrowserActionSetTitleFunction() override {}
};

class BrowserActionSetPopupFunction : public ExtensionActionSetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setPopup", BROWSERACTION_SETPOPUP)

 protected:
  ~BrowserActionSetPopupFunction() override {}
};

class BrowserActionGetTitleFunction : public ExtensionActionGetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getTitle", BROWSERACTION_GETTITLE)

 protected:
  ~BrowserActionGetTitleFunction() override {}
};

class BrowserActionGetPopupFunction : public ExtensionActionGetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getPopup", BROWSERACTION_GETPOPUP)

 protected:
  ~BrowserActionGetPopupFunction() override {}
};

class BrowserActionSetBadgeTextFunction
    : public ExtensionActionSetBadgeTextFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setBadgeText",
                             BROWSERACTION_SETBADGETEXT)

 protected:
  ~BrowserActionSetBadgeTextFunction() override {}
};

class BrowserActionSetBadgeBackgroundColorFunction
    : public ExtensionActionSetBadgeBackgroundColorFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.setBadgeBackgroundColor",
                             BROWSERACTION_SETBADGEBACKGROUNDCOLOR)

 protected:
  ~BrowserActionSetBadgeBackgroundColorFunction() override {}
};

class BrowserActionGetBadgeTextFunction
    : public ExtensionActionGetBadgeTextFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getBadgeText",
                             BROWSERACTION_GETBADGETEXT)

 protected:
  ~BrowserActionGetBadgeTextFunction() override {}
};

class BrowserActionGetBadgeBackgroundColorFunction
    : public ExtensionActionGetBadgeBackgroundColorFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.getBadgeBackgroundColor",
                             BROWSERACTION_GETBADGEBACKGROUNDCOLOR)

 protected:
  ~BrowserActionGetBadgeBackgroundColorFunction() override {}
};

class BrowserActionEnableFunction : public ExtensionActionShowFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.enable", BROWSERACTION_ENABLE)

 protected:
  ~BrowserActionEnableFunction() override {}
};

class BrowserActionDisableFunction : public ExtensionActionHideFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("browserAction.disable", BROWSERACTION_DISABLE)

 protected:
  ~BrowserActionDisableFunction() override {}
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
  ~PageActionShowFunction() override {}
};

class PageActionHideFunction : public extensions::ExtensionActionHideFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.hide", PAGEACTION_HIDE)

 protected:
  ~PageActionHideFunction() override {}
};

class PageActionSetIconFunction
    : public extensions::ExtensionActionSetIconFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.setIcon", PAGEACTION_SETICON)

 protected:
  ~PageActionSetIconFunction() override {}
};

class PageActionSetTitleFunction
    : public extensions::ExtensionActionSetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.setTitle", PAGEACTION_SETTITLE)

 protected:
  ~PageActionSetTitleFunction() override {}
};

class PageActionSetPopupFunction
    : public extensions::ExtensionActionSetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.setPopup", PAGEACTION_SETPOPUP)

 protected:
  ~PageActionSetPopupFunction() override {}
};

class PageActionGetTitleFunction
    : public extensions::ExtensionActionGetTitleFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.getTitle", PAGEACTION_GETTITLE)

 protected:
  ~PageActionGetTitleFunction() override {}
};

class PageActionGetPopupFunction
    : public extensions::ExtensionActionGetPopupFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("pageAction.getPopup", PAGEACTION_GETPOPUP)

 protected:
  ~PageActionGetPopupFunction() override {}
};

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_ACTION_EXTENSION_ACTION_API_H_
