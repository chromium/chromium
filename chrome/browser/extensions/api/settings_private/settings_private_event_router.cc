// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/settings_private/settings_private_event_router.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/settings_private/generated_prefs.h"
#include "chrome/browser/extensions/api/settings_private/generated_prefs_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/settings_private.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

SettingsPrivateEventRouter::SettingsPrivateEventRouter(
    content::BrowserContext* context)
    : context_(context) {
  Profile* profile = Profile::FromBrowserContext(context_);
  prefs_util_ = std::make_unique<PrefsUtil>(profile);
  user_prefs_registrar_.Init(profile->GetPrefs());
  local_state_registrar_.Init(g_browser_process->local_state());

  EventRouter::Get(context_)->RegisterObserver(
      this, api::settings_private::OnPrefsChanged::kEventName);
  StartOrStopListeningForPrefsChanges();
}

SettingsPrivateEventRouter::~SettingsPrivateEventRouter() {
  DCHECK(!listening_);
}

void SettingsPrivateEventRouter::OnGeneratedPrefChanged(
    const std::string& pref_name) {
  OnPreferenceChanged(pref_name);
}

void SettingsPrivateEventRouter::Shutdown() {
  EventRouter::Get(context_)->UnregisterObserver(this);

  if (listening_) {
#if defined(OS_CHROMEOS)
    cros_settings_subscription_map_.clear();
#endif
    const PrefsUtil::TypedPrefMap& keys = prefs_util_->GetWhitelistedKeys();
    settings_private::GeneratedPrefs* generated_prefs =
        settings_private::GeneratedPrefsFactory::GetForBrowserContext(context_);
    for (const auto& it : keys) {
      if (generated_prefs && generated_prefs->HasPref(it.first))
        generated_prefs->RemoveObserver(it.first, this);
      else if (!prefs_util_->IsCrosSetting(it.first))
        FindRegistrarForPref(it.first)->Remove(it.first);
    }
  }
  listening_ = false;
}

void SettingsPrivateEventRouter::OnListenerAdded(
    const EventListenerInfo& details) {
  // Start listening to events from the PrefChangeRegistrars.
  StartOrStopListeningForPrefsChanges();
}

void SettingsPrivateEventRouter::OnListenerRemoved(
    const EventListenerInfo& details) {
  // Stop listening to events from the PrefChangeRegistrars if there are no
  // more listeners.
  StartOrStopListeningForPrefsChanges();
}

PrefChangeRegistrar* SettingsPrivateEventRouter::FindRegistrarForPref(
    const std::string& pref_name) {
  Profile* profile = Profile::FromBrowserContext(context_);
  if (prefs_util_->FindServiceForPref(pref_name) == profile->GetPrefs()) {
    return &user_prefs_registrar_;
  }
  return &local_state_registrar_;
}

void SettingsPrivateEventRouter::StartOrStopListeningForPrefsChanges() {
  DCHECK(prefs_util_);
  EventRouter* event_router = EventRouter::Get(context_);
  bool should_listen = event_router->HasEventListener(
      api::settings_private::OnPrefsChanged::kEventName);

  settings_private::GeneratedPrefs* generated_prefs =
      settings_private::GeneratedPrefsFactory::GetForBrowserContext(context_);
  if (should_listen && !listening_) {
    const PrefsUtil::TypedPrefMap& keys = prefs_util_->GetWhitelistedKeys();
    for (const auto& it : keys) {
      std::string pref_name = it.first;
      if (prefs_util_->IsCrosSetting(pref_name)) {
#if defined(OS_CHROMEOS)
        std::unique_ptr<chromeos::CrosSettings::ObserverSubscription>
            subscription = chromeos::CrosSettings::Get()->AddSettingsObserver(
                pref_name.c_str(),
                base::Bind(&SettingsPrivateEventRouter::OnPreferenceChanged,
                           base::Unretained(this), pref_name));
        cros_settings_subscription_map_.insert(
            make_pair(pref_name, std::move(subscription)));
#endif
      } else if (generated_prefs && generated_prefs->HasPref(pref_name)) {
        generated_prefs->AddObserver(pref_name, this);
      } else {
        FindRegistrarForPref(it.first)
            ->Add(pref_name,
                  base::Bind(&SettingsPrivateEventRouter::OnPreferenceChanged,
                             base::Unretained(this)));
      }
    }
  } else if (!should_listen && listening_) {
    const PrefsUtil::TypedPrefMap& keys = prefs_util_->GetWhitelistedKeys();
    for (const auto& it : keys) {
      if (prefs_util_->IsCrosSetting(it.first)) {
#if defined(OS_CHROMEOS)
        cros_settings_subscription_map_.erase(it.first);
#endif
      } else if (generated_prefs && generated_prefs->HasPref(it.first)) {
        generated_prefs->RemoveObserver(it.first, this);
      } else {
        FindRegistrarForPref(it.first)->Remove(it.first);
      }
    }
  }
  listening_ = should_listen;
}

void SettingsPrivateEventRouter::OnPreferenceChanged(
    const std::string& pref_name) {
  // This posts an asynchronous task to ensure that all pref stores are updated,
  // as |prefs_util_->GetPref()| relies on this information to determine if a
  // preference is controlled by e.g. extensions.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&SettingsPrivateEventRouter::SendPrefChange,
                                weak_ptr_factory_.GetWeakPtr(), pref_name));
}

void SettingsPrivateEventRouter::SendPrefChange(const std::string& pref_name) {
  EventRouter* event_router = EventRouter::Get(context_);
  if (!event_router->HasEventListener(
          api::settings_private::OnPrefsChanged::kEventName)) {
    return;
  }

  std::unique_ptr<api::settings_private::PrefObject> pref_object =
      prefs_util_->GetPref(pref_name);

  std::vector<api::settings_private::PrefObject> prefs;
  if (pref_object)
    prefs.push_back(std::move(*pref_object));

  std::unique_ptr<base::ListValue> args(
      api::settings_private::OnPrefsChanged::Create(prefs));

  std::unique_ptr<Event> extension_event(new Event(
      events::SETTINGS_PRIVATE_ON_PREFS_CHANGED,
      api::settings_private::OnPrefsChanged::kEventName, std::move(args)));
  event_router->BroadcastEvent(std::move(extension_event));
}

SettingsPrivateEventRouter* SettingsPrivateEventRouter::Create(
    content::BrowserContext* context) {
  return new SettingsPrivateEventRouter(context);
}

}  // namespace extensions
