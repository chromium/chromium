// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_SYNC_CONSENT_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_SYNC_CONSENT_SCREEN_H_

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ui/webui/chromeos/login/sync_consent_screen_handler.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/user_manager/user.h"

class Profile;

namespace chromeos {

// This is Sync settings screen that is displayed as a part of user first
// sign-in flow.
class SyncConsentScreen : public BaseScreen,
                          public syncer::SyncServiceObserver {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. Public for testing. See
  // GetSyncScreenBehavior() for documentation on each case.
  enum class SyncScreenBehavior {
    kUnknown = 0,
    kShow = 1,
    kSkipNonGaiaAccount = 2,
    kSkipPublicAccount = 3,
    kSkipPermissionsPolicy = 4,
    kSkipAndEnableNonBrandedBuild = 5,
    kSkipAndEnableEmphemeralUser = 6,
    kSkipAndEnableScreenPolicy = 7,
    kMaxValue = kSkipAndEnableScreenPolicy
  };

  enum ConsentGiven { CONSENT_NOT_GIVEN, CONSENT_GIVEN };

  enum class Result { NEXT, NOT_APPLICABLE };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  class SyncConsentScreenTestDelegate {
   public:
    SyncConsentScreenTestDelegate() = default;

    // This is called from SyncConsentScreen when user consent is passed to
    // consent auditor with resource ids recorder as consent.
    virtual void OnConsentRecordedIds(
        ConsentGiven consent_given,
        const std::vector<int>& consent_description,
        int consent_confirmation) = 0;

    // This is called from SyncConsentScreenHandler when user consent is passed
    // to consent auditor with resource strings recorder as consent.
    virtual void OnConsentRecordedStrings(
        const ::login::StringList& consent_description,
        const std::string& consent_confirmation) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(SyncConsentScreenTestDelegate);
  };

  // Launches the sync consent settings dialog if the user requested to review
  // them after completing OOBE.
  static void MaybeLaunchSyncConsentSettings(Profile* profile);

  SyncConsentScreen(SyncConsentScreenView* view,
                    const ScreenExitCallback& exit_callback);
  ~SyncConsentScreen() override;

  // Inits `user_`, its `profile_` and `behavior_` before using the screen.
  void Init();

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

  // Reacts to "Continue and review settings after sign-in"
  void OnContinueAndReview(const std::vector<int>& consent_description,
                           const int consent_confirmation);

  // Reacts to "Continue with default settings"
  void OnContinueWithDefaults(const std::vector<int>& consent_description,
                              const int consent_confirmation);

  // Reacts to "Yes, I'm in" and "No, thanks".
  void OnContinue(const std::vector<int>& consent_description,
                  int consent_confirmation,
                  SyncConsentScreenHandler::UserChoice choice);

  // Configures OS sync and browser sync.
  void UpdateSyncSettings(bool enable_sync);

  // Enables sync if required when skipping the dialog.
  void MaybeEnableSyncForSkip();

  // Sets internal condition "Sync disabled by policy" for tests.
  void SetProfileSyncDisabledByPolicyForTesting(bool value);

  // Sets internal condition "Sync engine initialized" for tests.
  void SetProfileSyncEngineInitializedForTesting(bool value);

  // Test API.
  void SetDelegateForTesting(
      SyncConsentScreen::SyncConsentScreenTestDelegate* delegate);
  SyncConsentScreenTestDelegate* GetDelegateForTesting() const;

  void set_exit_callback_for_testing(const ScreenExitCallback& exit_callback) {
    exit_callback_ = exit_callback;
  }

  const ScreenExitCallback& get_exit_callback_for_testing() {
    return exit_callback_;
  }

 private:
  // Marks the dialog complete and runs `exit_callback_`.
  void Finish(Result result);

  // BaseScreen:
  bool MaybeSkip(WizardContext* context) override;
  void ShowImpl() override;
  void HideImpl() override;

  // Returns new SyncScreenBehavior value.
  SyncScreenBehavior GetSyncScreenBehavior() const;

  // Calculates updated `behavior_` and performs required update actions.
  void UpdateScreen();

  // Records user Sync consent.
  void RecordConsent(ConsentGiven consent_given,
                     const std::vector<int>& consent_description,
                     int consent_confirmation);

  // Returns true if profile sync is disabled by policy.
  bool IsProfileSyncDisabledByPolicy() const;

  // Returns true if profile sync has finished initialization.
  bool IsProfileSyncEngineInitialized() const;

  // Controls screen appearance.
  // Spinner is shown until sync status has been decided.
  SyncScreenBehavior behavior_ = SyncScreenBehavior::kUnknown;

  SyncConsentScreenView* const view_;
  ScreenExitCallback exit_callback_;

  // Manages sync service observer lifetime.
  ScopedObserver<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observer_{this};

  // Primary user ind his Profile (if screen is shown).
  const user_manager::User* user_ = nullptr;
  Profile* profile_ = nullptr;
  bool is_initialized_ = false;

  base::Optional<bool> test_sync_disabled_by_policy_;
  base::Optional<bool> test_sync_engine_initialized_;

  // Notify tests.
  SyncConsentScreenTestDelegate* test_delegate_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SyncConsentScreen);
};

}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove after //chrome/browser/chromeos
// source migration is finished.
using ::chromeos::SyncConsentScreen;

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_SYNC_CONSENT_SCREEN_H_
