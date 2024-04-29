// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_ONBOARDING_NUDGE_CONTROLLER_H_
#define ASH_SYSTEM_PHONEHUB_ONBOARDING_NUDGE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/phonehub/feature_status_provider.h"

class PrefRegistrySimple;

namespace base {
class Clock;
}

namespace multidevice {
class RemoteDeviceRef;
}

namespace views {
class View;
}

namespace ash {
// This class is responsible for displaying Phone Hub onboarding nudge when
// applicable.
class ASH_EXPORT OnboardingNudgeController
    : public phonehub::FeatureStatusProvider::Observer {
 public:
  OnboardingNudgeController(views::View* anchored_view,
                            base::RepeatingClosure stop_animation_callback,
                            base::RepeatingClosure start_animation_callback,
                            base::Clock* clock);
  OnboardingNudgeController(const OnboardingNudgeController&) = delete;
  OnboardingNudgeController& operator=(const OnboardingNudgeController&) =
      delete;
  ~OnboardingNudgeController() override;

  // Register `kPhoneHubNudgeLastTimeShown`, `kPhoneHubNudgeTotalAppearances`,
  //`kPhoneHubNudgeLastActionTime` `kPhoneHubNudgeLastClickTime` &
  //`kSyncedDevices`
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  static constexpr char kPhoneHubNudgeId[] = "PhoneHubNudge";

  // A time pref indicating the last time Phone Hub Nudge was shown.
  static constexpr char kPhoneHubNudgeLastShownTime[] =
      "ash.phone_hub_nudge_last_shown";

  // An integer pref indicating the total number of times Phone Hub nudge has
  // been shown.
  static constexpr char kPhoneHubNudgeTotalAppearances[] =
      "ash.phone_hub_nudge_total_appearances";

  // A time pref indicating the time an action (nudge or hover) was taken on the
  // nudge.
  static constexpr char kPhoneHubNudgeLastActionTime[] =
      "ash.phone_hub_nudge_last_action_time";

  // A time pref indicating the time a user clicks on the nudge.
  static constexpr char kPhoneHubNudgeLastClickTime[] =
      "ash.phone_hub_nudge_last_click_time";

  // List pref of all eligible hosts that have been discovered.
  static constexpr char kSyncedDevices[] = "ash.phone_hub_synced_devices";

  void ShowNudgeIfNeeded();
  void HideNudge();

  // Attempts recording nudge action metric when Phone Hub icon is activated.
  void MaybeRecordNudgeAction();

  void OnNudgeHoverStateChanged(bool is_hovering);
  void OnNudgeClicked();
  void OnNudgeDismissed();

 private:
  FRIEND_TEST_ALL_PREFIXES(OnboardingNudgeControllerTest,
                           AddToSyncedDeviceListWhenNewEligibleDeviceFound);
  FRIEND_TEST_ALL_PREFIXES(OnboardingNudgeControllerTest,
                           DoNotAddToSyncedDeviceListIfAlreadyFound);

  bool IsDeviceStoredInPref(const multidevice::RemoteDeviceRef& device);

  void AddToEligibleDevicesPref(const multidevice::RemoteDeviceRef& device);

  void ResetNudgePrefs();

  // FeatureStatusProvider::Observer:
  void OnFeatureStatusChanged() override {}
  void OnEligiblePhoneHubHostFound(
      const multidevice::RemoteDeviceRefList eligible_devices) override;

  bool IsInPhoneHubNudgeExperimentGroup();

  bool ShouldShowNudge();

  bool is_nudge_clicked_ = false;
  bool is_phone_hub_icon_clicked_ = false;
  raw_ptr<views::View> anchored_view_;
  base::RepeatingClosure stop_animation_callback_;
  base::RepeatingClosure start_animation_callback_;
  raw_ptr<base::Clock> clock_;

  base::WeakPtrFactory<OnboardingNudgeController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_ONBOARDING_NUDGE_CONTROLLER_H_
