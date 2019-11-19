// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_BASE_H_
#define ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_BASE_H_

#include <string>

#include "ash/public/mojom/assistant_state_controller.mojom.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/optional.h"
#include "chromeos/services/assistant/public/cpp/assistant_prefs.h"

class PrefChangeRegistrar;
class PrefService;

namespace ash {

// A checked observer which receives Assistant state change.
class ASH_PUBLIC_EXPORT AssistantStateObserver
    : public mojom::AssistantStateObserver,
      public base::CheckedObserver {
 public:
  AssistantStateObserver() = default;
  ~AssistantStateObserver() override = default;

  virtual void OnAssistantConsentStatusChanged(int consent_status) {}
  virtual void OnAssistantContextEnabled(bool enabled) {}
  virtual void OnAssistantSettingsEnabled(bool enabled) {}
  virtual void OnAssistantHotwordAlwaysOn(bool hotword_always_on) {}
  virtual void OnAssistantHotwordEnabled(bool enabled) {}
  virtual void OnAssistantLaunchWithMicOpen(bool launch_with_mic_open) {}
  virtual void OnAssistantNotificationEnabled(bool notification_enabled) {}

  // mojom::AssistantStateObserver:
  void OnAssistantStatusChanged(ash::mojom::AssistantState state) override {}
  void OnAssistantFeatureAllowedChanged(
      ash::mojom::AssistantAllowedState state) override {}
  void OnArcPlayStoreEnabledChanged(bool enabled) override {}
  void OnLocaleChanged(const std::string& locale) override {}
  void OnLockedFullScreenStateChanged(bool enabled) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantStateObserver);
};

// Plain data class that holds Assistant related prefs and states. This is
// shared by both the controller that controls these values and client proxy
// that caches these values locally. Please do not use this object directly.
// For ash/browser use |AssistantState| and for other threads use
// |AssistantStateProxy|.
class ASH_PUBLIC_EXPORT AssistantStateBase {
 public:
  AssistantStateBase();
  virtual ~AssistantStateBase();

  mojom::AssistantState assistant_state() const { return assistant_state_; }

  const base::Optional<bool>& settings_enabled() const {
    return settings_enabled_;
  }

  const base::Optional<int>& consent_status() const { return consent_status_; }

  const base::Optional<bool>& context_enabled() const {
    return context_enabled_;
  }

  const base::Optional<bool>& hotword_enabled() const {
    return hotword_enabled_;
  }

  const base::Optional<bool>& hotword_always_on() const {
    return hotword_always_on_;
  }

  const base::Optional<bool>& launch_with_mic_open() const {
    return launch_with_mic_open_;
  }

  const base::Optional<bool>& notification_enabled() const {
    return notification_enabled_;
  }

  const base::Optional<mojom::AssistantAllowedState>& allowed_state() const {
    return allowed_state_;
  }

  const base::Optional<std::string>& locale() const { return locale_; }

  const base::Optional<bool>& arc_play_store_enabled() const {
    return arc_play_store_enabled_;
  }

  const base::Optional<bool>& locked_full_screen_enabled() const {
    return locked_full_screen_enabled_;
  }

  std::string ToString() const;

  void AddObserver(AssistantStateObserver* observer);
  void RemoveObserver(AssistantStateObserver* observer);

  void RegisterPrefChanges(PrefService* pref_service);

 protected:
  void InitializeObserver(AssistantStateObserver* observer);
  void InitializeObserverMojom(mojom::AssistantStateObserver* observer);

  // Called when the related preferences are obtained from the pref service.
  void UpdateConsentStatus();
  void UpdateContextEnabled();
  void UpdateSettingsEnabled();
  void UpdateHotwordAlwaysOn();
  void UpdateHotwordEnabled();
  void UpdateLaunchWithMicOpen();
  void UpdateNotificationEnabled();

  // Called when new values of the listened states are received.
  void UpdateAssistantStatus(mojom::AssistantState state);
  void UpdateFeatureAllowedState(mojom::AssistantAllowedState state);
  void UpdateLocale(const std::string& locale);
  void UpdateArcPlayStoreEnabled(bool enabled);
  void UpdateLockedFullScreenState(bool enabled);

  mojom::AssistantState assistant_state_ = mojom::AssistantState::NOT_READY;

  // TODO(b/138679823): Maybe remove Optional for preference values.
  // Whether the Assistant is enabled in system settings. nullopt if the
  // data is not available yet.
  base::Optional<bool> settings_enabled_;

  // The status of the user's consent. nullopt if the data is not available yet.
  base::Optional<int> consent_status_;

  // Whether screen context is enabled. nullopt if the data is not available
  // yet.
  base::Optional<bool> context_enabled_;

  // Whether hotword listening is enabled.
  base::Optional<bool> hotword_enabled_;

  // Whether hotword listening is always on/only with power source. nullopt
  // if the data is not available yet.
  base::Optional<bool> hotword_always_on_;

  // Whether the Assistant should launch with mic open;
  base::Optional<bool> launch_with_mic_open_;

  // Whether notification is enabled.
  base::Optional<bool> notification_enabled_;

  // Whether the Assistant feature is allowed or disallowed for what reason.
  // nullopt if the data is not available yet.
  base::Optional<mojom::AssistantAllowedState> allowed_state_;

  base::Optional<std::string> locale_;

  // Whether play store is enabled. nullopt if the data is not available yet.
  base::Optional<bool> arc_play_store_enabled_;

  // Whether locked full screen state is enabled. nullopt if the data is not
  // available yet.
  base::Optional<bool> locked_full_screen_enabled_;

  // Observes user profile prefs for the Assistant.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::ObserverList<AssistantStateObserver> observers_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AssistantStateBase);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_BASE_H_
