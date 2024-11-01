// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_DISPATCHER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_DISPATCHER_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/common/extension_id.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace extensions {

class ExtensionAction;
class ExtensionPrefs;

class ExtensionActionDispatcher : public BrowserContextKeyedAPI {
 public:
  class Observer {
   public:
    // Called when there is a change to the given `extension_action`.
    // `web_contents` is the web contents that was affected, and
    // `browser_context` is the associated BrowserContext. (The latter is
    // included because ExtensionActionDispatcher is shared between normal and
    // incognito contexts, so `browser_context` may not equal
    // `browser_context_`.)
    virtual void OnExtensionActionUpdated(
        ExtensionAction* extension_action,
        content::WebContents* web_contents,
        content::BrowserContext* browser_context) {}

    // Called when the ExtensionActionDispatcher is shutting down, giving
    // observers a chance to unregister themselves if there is not a definitive
    // lifecycle.
    virtual void OnShuttingDown() {}

   protected:
    virtual ~Observer() = default;
  };

  explicit ExtensionActionDispatcher(content::BrowserContext* context);

  ExtensionActionDispatcher(const ExtensionActionDispatcher&) = delete;
  ExtensionActionDispatcher& operator=(const ExtensionActionDispatcher&) =
      delete;

  ~ExtensionActionDispatcher() override;

  // Convenience method to get the instance for a profile.
  static ExtensionActionDispatcher* Get(content::BrowserContext* context);

  static BrowserContextKeyedAPIFactory<ExtensionActionDispatcher>*
  GetFactoryInstance();

  // Add or remove observers.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Notifies that there has been a change in the given `extension_action`.
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
  // given `web_contents` (and signals that page actions changed).
  void ClearAllValuesForTab(content::WebContents* web_contents);

  void set_prefs_for_testing(ExtensionPrefs* prefs) {
    extension_prefs_ = prefs;
  }

 private:
  friend class BrowserContextKeyedAPIFactory<ExtensionActionDispatcher>;

  // Returns the associated extension prefs.
  ExtensionPrefs* GetExtensionPrefs();

  // The DispatchEvent methods forward events to the `context`'s event router.
  void DispatchEventToExtension(content::BrowserContext* context,
                                const ExtensionId& extension_id,
                                events::HistogramValue histogram_value,
                                const std::string& event_name,
                                base::Value::List event_args);

  // BrowserContextKeyedAPI implementation.
  void Shutdown() override;
  static const char* service_name() { return "ExtensionActionDispatcher"; }
  static const bool kServiceRedirectedInIncognito = true;

  base::ObserverList<Observer>::Unchecked observers_;

  raw_ptr<content::BrowserContext> browser_context_ = nullptr;

  raw_ptr<ExtensionPrefs> extension_prefs_ = nullptr;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_ACTION_DISPATCHER_H_
