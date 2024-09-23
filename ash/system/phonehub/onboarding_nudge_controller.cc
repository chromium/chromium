// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/onboarding_nudge_controller.h"

#include <algorithm>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "ash/system/phonehub/phone_hub_ui_controller.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/clock.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "chromeos/ash/components/phonehub/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {
PrefService* GetPrefService() {
  return Shell::Get()->session_controller()->GetActivePrefService();
}
}  // namespace

// static
void OnboardingNudgeController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterTimePref(kPhoneHubNudgeLastShownTime, base::Time());
  registry->RegisterIntegerPref(kPhoneHubNudgeTotalAppearances, 0);
  registry->RegisterTimePref(kPhoneHubNudgeLastActionTime, base::Time());
  registry->RegisterTimePref(kPhoneHubNudgeLastClickTime, base::Time());
  registry->RegisterListPref(kSyncedDevices);
}

OnboardingNudgeController::OnboardingNudgeController(
    views::View* anchored_view,
    base::RepeatingClosure stop_animation_callback,
    base::RepeatingClosure start_animation_callback,
    base::Clock* clock)
    : anchored_view_(anchored_view),
      stop_animation_callback_(std::move(stop_animation_callback)),
      start_animation_callback_(std::move(start_animation_callback)),
      clock_(clock) {}

OnboardingNudgeController::~OnboardingNudgeController() = default;

void OnboardingNudgeController::ShowNudgeIfNeeded() {
  if (!IsInPhoneHubNudgeExperimentGroup()) {
    return;
  }

  if (!ShouldShowNudge()) {
    return;
  }
  PA_LOG(INFO)
      << "Phone Hub onboarding nudge is being shown for text experiment group "
      << (features::kPhoneHubNotifierTextGroup.Get() ==
                  features::PhoneHubNotifierTextGroup::kNotifierTextGroupA
              ? "A."
              : "B.");

  std::u16string nudge_text = l10n_util::GetStringUTF16(
      features::kPhoneHubNotifierTextGroup.Get() ==
              features::PhoneHubNotifierTextGroup::kNotifierTextGroupA
          ? IDS_ASH_MULTI_DEVICE_SETUP_NOTIFIER_TEXT_WITH_PHONE_HUB
          : IDS_ASH_MULTI_DEVICE_SETUP_NOTIFIER_TEXT_WITHOUT_PHONE_HUB);
  AnchoredNudgeData nudge_data = {kPhoneHubNudgeId, NudgeCatalogName::kPhoneHub,
                                  nudge_text, anchored_view_};
  nudge_data.anchored_to_shelf = true;
  nudge_data.hover_changed_callback =
      base::BindRepeating(&OnboardingNudgeController::OnNudgeHoverStateChanged,
                          weak_ptr_factory_.GetWeakPtr());
  nudge_data.click_callback =
      base::BindRepeating(&OnboardingNudgeController::OnNudgeClicked,
                          weak_ptr_factory_.GetWeakPtr());
  nudge_data.dismiss_callback = stop_animation_callback_.Then(
      base::BindRepeating(&OnboardingNudgeController::OnNudgeDismissed,
                          weak_ptr_factory_.GetWeakPtr()));
  AnchoredNudgeManager::Get()->Show(nudge_data);

  if (AnchoredNudgeManager::Get()->IsNudgeShown(kPhoneHubNudgeId)) {
    start_animation_callback_.Run();
    PrefService* pref_service = GetPrefService();
    pref_service->SetTime(kPhoneHubNudgeLastShownTime, clock_->Now());
    pref_service->SetInteger(
        kPhoneHubNudgeTotalAppearances,
        pref_service->GetInteger(kPhoneHubNudgeTotalAppearances) + 1);
    base::UmaHistogramCounts100("MultiDeviceSetup.NudgeShown", 1);
  }
}

void OnboardingNudgeController::HideNudge() {
  if (!IsInPhoneHubNudgeExperimentGroup()) {
    return;
  }
  if (AnchoredNudgeManager::Get()->IsNudgeShown(kPhoneHubNudgeId)) {
    // `HideNudge()` is only invoked when Phone Hub icon is clicked. If the
    // nudge is visible, it should be counted as interaction.
    PrefService* pref_service = GetPrefService();
    base::Time time = clock_->Now();
    pref_service->SetTime(kPhoneHubNudgeLastActionTime, time);
    pref_service->SetTime(kPhoneHubNudgeLastClickTime, time);
    is_phone_hub_icon_clicked_ = true;
    AnchoredNudgeManager::Get()->Cancel(kPhoneHubNudgeId);
  }
}

void OnboardingNudgeController::MaybeRecordNudgeAction() {
  if (!IsInPhoneHubNudgeExperimentGroup()) {
    return;
  }
  AnchoredNudgeManager::Get()->MaybeRecordNudgeAction(
      NudgeCatalogName::kPhoneHub);
}

void OnboardingNudgeController::OnNudgeHoverStateChanged(bool is_hovering) {
  if (!IsInPhoneHubNudgeExperimentGroup()) {
    return;
  }

  if (is_hovering) {
    PrefService* pref_service = GetPrefService();
    pref_service->SetTime(kPhoneHubNudgeLastActionTime, clock_->Now());
  }
}

void OnboardingNudgeController::OnNudgeClicked() {
  if (!IsInPhoneHubNudgeExperimentGroup()) {
    return;
  }

  is_nudge_clicked_ = true;

  // Action can be click or hover so define `kPhoneHubNudgeLastActionTime` on
  // click, but `kPhoneHubNudgeLastClickTime` should be set separately.
  PrefService* pref_service = GetPrefService();
  base::Time time = clock_->Now();
  pref_service->SetTime(kPhoneHubNudgeLastActionTime, time);
  pref_service->SetTime(kPhoneHubNudgeLastClickTime, time);
}

void OnboardingNudgeController::OnNudgeDismissed() {
  if (!IsInPhoneHubNudgeExperimentGroup()) {
    return;
  }

  if (is_nudge_clicked_ || is_phone_hub_icon_clicked_) {
    PrefService* pref_service = GetPrefService();
    base::UmaHistogramTimes(
        "MultiDeviceSetup.NudgeActionDuration",
        pref_service->GetTime(kPhoneHubNudgeLastClickTime) -
            pref_service->GetTime(kPhoneHubNudgeLastShownTime));
    base::UmaHistogramCounts100(
        "MultiDeviceSetup.NudgeShownTimesBeforeActed",
        pref_service->GetInteger(kPhoneHubNudgeTotalAppearances));
    if (is_nudge_clicked_) {
      base::UmaHistogramEnumeration(
          "MultiDeviceSetup.NudgeInteracted",
          phone_hub_metrics::MultideviceSetupNudgeInteraction::kNudgeClicked);
    }
    if (is_phone_hub_icon_clicked_) {
      base::UmaHistogramEnumeration(
          "MultiDeviceSetup.NudgeInteracted",
          phone_hub_metrics::MultideviceSetupNudgeInteraction::
              kPhoneHubIconClicked);
    }
  }
  is_nudge_clicked_ = false;
  is_phone_hub_icon_clicked_ = false;
}

void OnboardingNudgeController::OnEligiblePhoneHubHostFound(
    const multidevice::RemoteDeviceRefList eligible_devices) {
  bool new_host_found = false;
  for (const multidevice::RemoteDeviceRef& device : eligible_devices) {
    if (!IsDeviceStoredInPref(device)) {
      AddToEligibleDevicesPref(device);
      new_host_found = true;
    }
  }
  if (new_host_found) {
    // Rest pref values to show the nudge again when user gets a new eligible
    // phone.
    ResetNudgePrefs();
  }
}

void OnboardingNudgeController::AddToEligibleDevicesPref(
    const multidevice::RemoteDeviceRef& device) {
  PrefService* pref_service = GetPrefService();
  const base::Value::List& devices_in_pref =
      pref_service->GetList(kSyncedDevices);
  base::Value::List updated_device_list = devices_in_pref.Clone();
  updated_device_list.Append(device.instance_id());
  pref_service->SetList(kSyncedDevices, updated_device_list.Clone());
}

void OnboardingNudgeController::ResetNudgePrefs() {
  PrefService* pref_service = GetPrefService();
  if (pref_service->FindPreference(phonehub::prefs::kHideOnboardingUi)) {
    pref_service->SetBoolean(phonehub::prefs::kHideOnboardingUi, false);
  }
  pref_service->SetInteger(kPhoneHubNudgeTotalAppearances, 0);
  pref_service->SetTime(kPhoneHubNudgeLastShownTime, base::Time());
  pref_service->SetTime(kPhoneHubNudgeLastActionTime, base::Time());
  pref_service->SetTime(kPhoneHubNudgeLastClickTime, base::Time());
}

bool OnboardingNudgeController::IsDeviceStoredInPref(
    const multidevice::RemoteDeviceRef& device) {
  PrefService* pref_service = GetPrefService();
  const base::Value::List& devices_in_pref =
      pref_service->GetList(kSyncedDevices);
  return base::Contains(devices_in_pref, base::Value(device.instance_id()));
}

bool OnboardingNudgeController::IsInPhoneHubNudgeExperimentGroup() {
  return features::IsPhoneHubOnboardingNotifierRevampEnabled() &&
         features::kPhoneHubOnboardingNotifierUseNudge.Get();
}

bool OnboardingNudgeController::ShouldShowNudge() {
  PrefService* pref_service = GetPrefService();
  if (!pref_service->GetTime(kPhoneHubNudgeLastClickTime).is_null()) {
    // User has taken actions on the nudge. We do not show it to them again.
    return false;
  }

  if (pref_service->GetInteger(kPhoneHubNudgeTotalAppearances) >=
      features::kPhoneHubNudgeTotalAppearancesAllowed.Get()) {
    PA_LOG(INFO) << "Nudge has been shown "
                 << features::kPhoneHubNudgeTotalAppearancesAllowed.Get()
                 << " times. Do not show again.";
    return false;
  }
  // Nudge has not been shown before.
  if (pref_service->GetTime(kPhoneHubNudgeLastShownTime).is_null()) {
    return true;
  }

  if ((clock_->Now() - pref_service->GetTime(kPhoneHubNudgeLastShownTime)) >=
      features::kPhoneHubNudgeDelay.Get()) {
    return true;
  }
  PA_LOG(INFO) << "Nudge was shown less than "
               << features::kPhoneHubNudgeDelay.Get()
               << " hours ago. Not being shown this time.";
  return false;
}
}  // namespace ash
