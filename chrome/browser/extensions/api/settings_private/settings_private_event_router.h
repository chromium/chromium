// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_EVENT_ROUTER_H_

#include <map>
#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/extensions/api/settings_private/generated_pref.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "extensions/browser/event_router.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/settings/cros_settings.h"
#endif

namespace content {
class BrowserContext;
}

namespace extensions {

// This is an event router that will observe listeners to pref changes on the
// appropriate pref service(s) and notify listeners on the JavaScript
// settingsPrivate API.
class SettingsPrivateEventRouter
    : public KeyedService,
      public EventRouter::Observer,
      public settings_private::GeneratedPref::Observer {
 public:
  static std::unique_ptr<SettingsPrivateEventRouter> Create(
      content::BrowserContext* browser_context);

  explicit SettingsPrivateEventRouter(content::BrowserContext* context);
  SettingsPrivateEventRouter(const SettingsPrivateEventRouter&) = delete;
  SettingsPrivateEventRouter& operator=(const SettingsPrivateEventRouter&) =
      delete;

  ~SettingsPrivateEventRouter() override;

  // settings_private::GeneratedPref::Observer implementation.
  void OnGeneratedPrefChanged(const std::string& pref_name) override;

  content::BrowserContext* context_for_test() { return context_; }

 protected:
  // KeyedService overrides:
  void Shutdown() override;

  // EventRouter::Observer overrides:
  void OnListenerAdded(const EventListenerInfo& details) override;
  void OnListenerRemoved(const EventListenerInfo& details) override;

  // This registrar monitors for user prefs changes.
  PrefChangeRegistrar user_prefs_registrar_;

  // This registrar monitors for local state changes.
  PrefChangeRegistrar local_state_registrar_;

 private:
  // Decide if we should listen for pref changes or not. If there are any
  // JavaScript listeners registered for the onPrefsChanged event, then we
  // want to register for change notification from the PrefChangeRegistrar.
  // Otherwise, we want to unregister and not be listening for pref changes.
  void StartOrStopListeningForPrefsChanges();

  void OnPreferenceChanged(const std::string& pref_name);

  // Sends a pref change to any listeners (if they exist; no-ops otherwise).
  void SendPrefChange(const std::string& pref_name);

  PrefChangeRegistrar* FindRegistrarForPref(const std::string& pref_name);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  using SubscriptionMap = std::map<std::string, base::CallbackListSubscription>;
  SubscriptionMap cros_settings_subscription_map_;
#endif

  const raw_ptr<content::BrowserContext> context_;
  bool listening_ = false;

  std::unique_ptr<PrefsUtil> prefs_util_;

  base::WeakPtrFactory<SettingsPrivateEventRouter> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SETTINGS_PRIVATE_SETTINGS_PRIVATE_EVENT_ROUTER_H_
