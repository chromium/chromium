// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_CONSOLIDATED_CONSENT_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_CONSOLIDATED_CONSENT_SCREEN_H_

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ash/arc/optin/arc_optin_preference_handler_observer.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/settings/device_settings_service.h"
#include "chrome/browser/ui/webui/ash/login/consolidated_consent_screen_handler.h"

namespace arc {
class ArcOptInPreferenceHandler;
}

namespace ash {

class ScopedSessionRefresher;

// Controller for the consolidated consent screen.
class ConsolidatedConsentScreen
    : public BaseScreen,
      public arc::ArcOptInPreferenceHandlerObserver {
 public:
  enum class Result {
    // The user accepted terms of service in the regular flow.
    ACCEPTED,

    // The user clicked the back button in the demo mode.
    BACK_DEMO,

    // The user accepted terms of service in online demo mode.
    ACCEPTED_DEMO_ONLINE,

    // Consolidated Consent screen skipped.
    NOT_APPLICABLE,
  };

  // The result of the cryptohome recovery opt-in.
  // These values are logged to UMA
  // ("OOBE.ConsolidatedConsentScreen.RecoveryOptInResult"). Entries should not
  // be renumbered and numeric values should never be reused.
  enum class RecoveryOptInResult {
    kNotSupported = 0,
    kUserOptIn = 1,
    kUserOptOut = 2,
    kPolicyOptIn = 3,
    kPolicyOptOut = 4,
    kMaxValue = kPolicyOptOut,
  };

  class Observer : public base::CheckedObserver {
   public:
    // Called when the user accepts terms of service.
    virtual void OnConsolidatedConsentAccept() = 0;
    virtual void OnConsolidatedConsentScreenDestroyed() = 0;
  };

  using TView = ConsolidatedConsentScreenView;
  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  ConsolidatedConsentScreen(base::WeakPtr<ConsolidatedConsentScreenView> view,
                            const ScreenExitCallback& exit_callback);
  ~ConsolidatedConsentScreen() override;
  ConsolidatedConsentScreen(const ConsolidatedConsentScreen&) = delete;
  ConsolidatedConsentScreen& operator=(const ConsolidatedConsentScreen&) =
      delete;

  static std::string GetResultString(Result result);

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void OnAccept(bool enable_stats_usage,
                bool enable_backup_restore,
                bool enable_location_services,
                const std::string& tos_content,
                bool enable_recovery);

  // arc::ArcOptInPreferenceHandlerObserver:
  void OnMetricsModeChanged(bool enabled, bool managed) override;
  void OnBackupAndRestoreModeChanged(bool enabled, bool managed) override;
  void OnLocationServicesModeChanged(bool enabled, bool managed) override;

  // Called by unit tests to notify observers that the user aceepted the terms
  // of service.
  void NotifyConsolidatedConsentAcceptForTesting();

 protected:
  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;
  ScreenExitCallback* exit_callback() { return &exit_callback_; }

 private:
  struct ConsentsParameters {
    std::string tos_content;
    bool record_arc_tos_consent;
    bool record_backup_consent;
    bool backup_accepted;
    bool record_location_consent;
    bool location_accepted;
  };

  void RecordConsents(const ConsentsParameters& params);

  void OnOwnershipStatusCheckDone(
      DeviceSettingsService::OwnershipStatus status);

  void ReportUsageOptIn(bool is_enabled);

  // Exits the screen with `Result::ACCEPTED` in the normal flow, and
  // `Result::ACCEPTED_DEMO_ONLINE` or `Result::ACCEPTED_DEMO_OFFLINE` in the
  // demo setup flow.
  void ExitScreenWithAcceptedResult();

  // Updates the state of the metrics toggle.
  void UpdateMetricsMode(bool enabled, bool managed);

  std::optional<bool> is_owner_;

  bool is_child_account_ = false;

  // To track if optional ARC features are managed preferences.
  bool backup_restore_managed_ = false;
  bool location_services_managed_ = false;

  base::ObserverList<Observer, true> observer_list_;

  // Keeps cryptohome authsession alive.
  std::unique_ptr<ScopedSessionRefresher> session_refresher_;

  std::unique_ptr<arc::ArcOptInPreferenceHandler> pref_handler_;

  base::WeakPtr<ConsolidatedConsentScreenView> view_;

  ScreenExitCallback exit_callback_;

  base::WeakPtrFactory<ConsolidatedConsentScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_CONSOLIDATED_CONSENT_SCREEN_H_
