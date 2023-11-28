// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_BASE_H_
#define ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_BASE_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_enums.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_prefs.h"

class PrefChangeRegistrar;
class PrefService;

namespace ash {

// A checked observer which receives Assistant state change.
class ASH_PUBLIC_EXPORT AssistantStateObserver : public base::CheckedObserver {
 public:
  AssistantStateObserver() = default;

  AssistantStateObserver(const AssistantStateObserver&) = delete;
  AssistantStateObserver& operator=(const AssistantStateObserver&) = delete;

  ~AssistantStateObserver() override = default;

  virtual void OnAssistantConsentStatusChanged(int consent_status) {}
  virtual void OnAssistantContextEnabled(bool enabled) {}
  virtual void OnAssistantSettingsEnabled(bool enabled) {}
  virtual void OnAssistantHotwordAlwaysOn(bool hotword_always_on) {}
  virtual void OnAssistantHotwordEnabled(bool enabled) {}
  virtual void OnAssistantLaunchWithMicOpen(bool launch_with_mic_open) {}
  virtual void OnAssistantNotificationEnabled(bool notification_enabled) {}
  virtual void OnAssistantOnboardingModeChanged(
      assistant::prefs::AssistantOnboardingMode onboarding_mode) {}
  virtual void OnAssistantStateDestroyed() {}
  virtual void OnAssistantStatusChanged(assistant::AssistantStatus status) {}
  virtual void OnAssistantFeatureAllowedChanged(
      assistant::AssistantAllowedState state) {}
  virtual void OnArcPlayStoreEnabledChanged(bool enabled) {}
  virtual void OnLocaleChanged(const std::string& locale) {}
  virtual void OnLockedFullScreenStateChanged(bool enabled) {}
};

// Plain data class that holds Assistant related prefs and states. This is
// shared by both the controller that controls these values and client proxy
// that caches these values locally. Please do not use this object directly.
// For ash/browser use |AssistantState| and for other threads use
// |AssistantStateProxy|.
class ASH_PUBLIC_EXPORT AssistantStateBase {
 public:
  AssistantStateBase();

  AssistantStateBase(const AssistantStateBase&) = delete;
  AssistantStateBase& operator=(const AssistantStateBase&) = delete;

  virtual ~AssistantStateBase();

  assistant::AssistantStatus assistant_status() const {
    return assistant_status_;
  }

  const std::optional<bool>& settings_enabled() const {
    return settings_enabled_;
  }

  const std::optional<int>& consent_status() const { return consent_status_; }

  const std::optional<bool>& context_enabled() const {
    return context_enabled_;
  }

  const std::optional<bool>& hotword_enabled() const {
    return hotword_enabled_;
  }

  const std::optional<bool>& hotword_always_on() const {
    return hotword_always_on_;
  }

  const std::optional<bool>& launch_with_mic_open() const {
    return launch_with_mic_open_;
  }

  const std::optional<bool>& notification_enabled() const {
    return notification_enabled_;
  }

  const std::optional<assistant::prefs::AssistantOnboardingMode>&
  onboarding_mode() const {
    return onboarding_mode_;
  }

  const std::optional<assistant::AssistantAllowedState>& allowed_state() const {
    return allowed_state_;
  }

  const std::optional<std::string>& locale() const { return locale_; }

  const std::optional<bool>& arc_play_store_enabled() const {
    return arc_play_store_enabled_;
  }

  const std::optional<bool>& locked_full_screen_enabled() const {
    return locked_full_screen_enabled_;
  }

  std::string ToString() const;

  void AddObserver(AssistantStateObserver* observer);
  void RemoveObserver(AssistantStateObserver* observer);

  void RegisterPrefChanges(PrefService* pref_service);

  bool IsScreenContextAllowed() const;

  bool HasAudioInputDevice() const;

 protected:
  void InitializeObserver(AssistantStateObserver* observer);

  // Called when the related preferences are obtained from the pref service.
  void UpdateConsentStatus();
  void UpdateContextEnabled();
  void UpdateSettingsEnabled();
  void UpdateHotwordAlwaysOn();
  void UpdateHotwordEnabled();
  void UpdateLaunchWithMicOpen();
  void UpdateNotificationEnabled();
  void UpdateOnboardingMode();

  // Called when new values of the listened states are received.
  void UpdateAssistantStatus(assistant::AssistantStatus status);
  void UpdateFeatureAllowedState(assistant::AssistantAllowedState state);
  void UpdateLocale(const std::string& locale);
  void UpdateArcPlayStoreEnabled(bool enabled);
  void UpdateLockedFullScreenState(bool enabled);

  assistant::AssistantStatus assistant_status_ =
      assistant::AssistantStatus::NOT_READY;

  // TODO(b/138679823): Maybe remove Optional for preference values.
  // Whether the Assistant is enabled in system settings. nullopt if the
  // data is not available yet.
  std::optional<bool> settings_enabled_;

  // The status of the user's consent. nullopt if the data is not available yet.
  std::optional<int> consent_status_;

  // Whether screen context is enabled. nullopt if the data is not available
  // yet.
  std::optional<bool> context_enabled_;

  // Whether hotword listening is enabled.
  std::optional<bool> hotword_enabled_;

  // Whether hotword listening is always on/only with power source. nullopt
  // if the data is not available yet.
  std::optional<bool> hotword_always_on_;

  // Whether the Assistant should launch with mic open;
  std::optional<bool> launch_with_mic_open_;

  // Whether notification is enabled.
  std::optional<bool> notification_enabled_;

  // The mode for the Assistant onboarding experience.
  std::optional<assistant::prefs::AssistantOnboardingMode> onboarding_mode_;

  // Whether the Assistant feature is allowed or disallowed for what reason.
  // nullopt if the data is not available yet.
  std::optional<assistant::AssistantAllowedState> allowed_state_;

  std::optional<std::string> locale_;

  // Whether play store is enabled. nullopt if the data is not available yet.
  std::optional<bool> arc_play_store_enabled_;

  // Whether locked full screen state is enabled. nullopt if the data is not
  // available yet.
  std::optional<bool> locked_full_screen_enabled_;

  // Observes user profile prefs for the Assistant.
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::ObserverList<AssistantStateObserver> observers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_ASSISTANT_STATE_BASE_H_
