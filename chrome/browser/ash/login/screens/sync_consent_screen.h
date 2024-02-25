// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_SCREENS_SYNC_CONSENT_SCREEN_H_
#define CHROME_BROWSER_ASH_LOGIN_SCREENS_SYNC_CONSENT_SCREEN_H_

#include <memory>
#include <optional>
#include <string>

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/user_manager/user.h"

class Profile;

namespace ash {

class SyncConsentScreenView;
class ScopedSessionRefresher;

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

  enum class Result { NEXT, DECLINE, NOT_APPLICABLE };

  static std::string GetResultString(Result result);

  using ScreenExitCallback = base::RepeatingCallback<void(Result result)>;

  class SyncConsentScreenTestDelegate {
   public:
    SyncConsentScreenTestDelegate() = default;

    SyncConsentScreenTestDelegate(const SyncConsentScreenTestDelegate&) =
        delete;
    SyncConsentScreenTestDelegate& operator=(
        const SyncConsentScreenTestDelegate&) = delete;

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
  };

  class SyncConsentScreenExitTestDelegate {
   public:
    virtual ~SyncConsentScreenExitTestDelegate() = default;

    virtual void OnSyncConsentScreenExit(
        Result result,
        ScreenExitCallback& original_callback) = 0;
  };

  // Launches the sync consent settings dialog if the user requested to review
  // them after completing OOBE.
  static void MaybeLaunchSyncConsentSettings(Profile* profile);

  SyncConsentScreen(base::WeakPtr<SyncConsentScreenView> view,
                    const ScreenExitCallback& exit_callback);

  SyncConsentScreen(const SyncConsentScreen&) = delete;
  SyncConsentScreen& operator=(const SyncConsentScreen&) = delete;

  ~SyncConsentScreen() override;

  // Inits `user_`, its `profile_` and `behavior_` before using the screen.
  void Init(const WizardContext& context);

  // syncer::SyncServiceObserver:
  void OnStateChanged(syncer::SyncService* sync) override;

  // Reacts to user action on sync.
  void OnContinue(const bool opted_in,
                  const bool review_sync,
                  const std::vector<int>& consent_description,
                  const int consent_confirmation);

  // Enables sync if required when skipping the dialog.
  void MaybeEnableSyncForSkip();

  // Called when sync engine initialization timed out.
  void OnTimeout();

  void OnAshContinue(const bool opted_in,
                     const bool review_sync,
                     const base::Value::List& consent_description_list,
                     const std::string& consent_confirmation);

  void OnLacrosContinue(const base::Value::List& consent_description_list,
                        const std::string& consent_confirmation);

  void RecordAllConsents(const bool opted_in,
                         const base::Value::List& consent_description_list,
                         const std::string& consent_confirmation);

  // Sets internal condition "Sync disabled by policy" for tests.
  static void SetProfileSyncDisabledByPolicyForTesting(bool value);

  // Sets internal condition "Sync engine initialized" for tests.
  static void SetProfileSyncEngineInitializedForTesting(bool value);

  // Test API.
  void SetDelegateForTesting(
      SyncConsentScreen::SyncConsentScreenTestDelegate* delegate);
  SyncConsentScreenTestDelegate* GetDelegateForTesting() const;

  // When set, test callback will be called instead of the |exit_callback_|.
  static void SetSyncConsentScreenExitTestDelegate(
      SyncConsentScreenExitTestDelegate* test_delegate);

  // Test API
  // Returns true if profile sync is disabled by policy for test.
  bool IsProfileSyncDisabledByPolicyForTest() const;

 private:
  // Marks the dialog complete and runs `exit_callback_`.
  void Finish(Result result);

  // BaseScreen:
  bool MaybeSkip(WizardContext& context) override;
  void ShowImpl() override;
  void HideImpl() override;
  void OnUserAction(const base::Value::List& args) override;

  // Returns new SyncScreenBehavior value.
  SyncScreenBehavior GetSyncScreenBehavior(const WizardContext& context) const;

  // Calculates updated `behavior_` and performs required update actions.
  void UpdateScreen(const WizardContext& context);

  // Records user Sync consent.
  void RecordConsent(ConsentGiven consent_given,
                     const std::vector<int>& consent_description,
                     int consent_confirmation);

  // Returns true if profile sync is disabled by policy.
  bool IsProfileSyncDisabledByPolicy() const;

  // Returns true if profile sync has finished initialization.
  bool IsProfileSyncEngineInitialized() const;

  // Check if OSSyncRevamp and Lacros are enabled.
  bool IsOsSyncLacros();

  // This function does two things based on account capability: turn on "sync
  // everything" toggle for non-minor users; pass the minor mode signal to
  // the front end, which controls whether nudge techniques could be used.
  void PrepareScreenBasedOnCapability();

  // Set "sync everything" toggle to be on or off. We also turn off all data
  // types when the toggle is off.
  void SetSyncEverythingEnabled(bool enabled);

  // Controls screen appearance.
  // Spinner is shown until sync status has been decided.
  SyncScreenBehavior behavior_ = SyncScreenBehavior::kUnknown;

  base::WeakPtr<SyncConsentScreenView> view_;
  ScreenExitCallback exit_callback_;

  // Manages sync service observer lifetime.
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};

  // Keeps cryptohome authsession alive.
  std::unique_ptr<ScopedSessionRefresher> session_refresher_;

  // Primary user ind his Profile (if screen is shown).
  raw_ptr<const user_manager::User> user_ = nullptr;
  raw_ptr<Profile> profile_ = nullptr;
  bool is_initialized_ = false;

  // Used to record whether sync engine initialization is timed out.
  base::OneShotTimer timeout_waiter_;
  bool is_timed_out_ = false;

  // The time when sync consent screen starts loading.
  base::TimeTicks start_time_;

  // Notify tests.
  raw_ptr<SyncConsentScreenTestDelegate> test_delegate_ = nullptr;

  base::WeakPtrFactory<SyncConsentScreen> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_SCREENS_SYNC_CONSENT_SCREEN_H_
