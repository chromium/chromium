// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_nudge_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/mahi/mahi_constants.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "chromeos/components/magic_boost/public/cpp/magic_boost_state.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

PrefService* GetPrefService() {
  return Shell::Get()->session_controller()->GetActivePrefService();
}

}  // namespace

MahiNudgeController::MahiNudgeController() = default;

MahiNudgeController::~MahiNudgeController() = default;

// static
void MahiNudgeController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(
      prefs::kMahiNudgeShownCount, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_OS_PREF);
  registry->RegisterTimePref(prefs::kMahiNudgeLastShownTime, base::Time());
}

void MahiNudgeController::MaybeShowNudge() {
  auto* pref_service = GetPrefService();
  auto* magic_boost_state = chromeos::MagicBoostState::Get();

  // Don't show nudge if the feature has been enabled by the user.
  if (magic_boost_state->hmr_enabled().has_value() &&
      magic_boost_state->hmr_enabled().value()) {
    return;
  }

  // Don't show nudge if users has explicitly interacted with the feature
  // consent status (they have explicitly made a decision to not use the
  // feature).
  if (magic_boost_state->hmr_consent_status() !=
      chromeos::HMRConsentStatus::kUnset) {
    return;
  }

  // Nudge has been shown three times. No need to educate anymore.
  const int shown_count = pref_service->GetInteger(prefs::kMahiNudgeShownCount);
  if (shown_count >= mahi_constants::kNudgeMaxShownCount) {
    return;
  }

  // Don't show nudge if it was shown within the last 24 hours.
  if (base::Time::Now() -
          pref_service->GetTime(prefs::kMahiNudgeLastShownTime) <
      mahi_constants::kNudgeTimeBetweenShown) {
    return;
  }

  AnchoredNudgeData nudge_data(
      mahi_constants::kMahiNudgeId, NudgeCatalogName::kMahi,
      /*body_text=*/
      l10n_util::GetStringUTF16(IDS_ASH_MAHI_EDUCATIONAL_NUDGE_BODY));
  nudge_data.title_text =
      l10n_util::GetStringUTF16(IDS_ASH_MAHI_EDUCATIONAL_NUDGE_TITLE);
  nudge_data.image_model =
      ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_MAHI_EDUCATIONAL_NUDGE_IMAGE);

  Shell::Get()->anchored_nudge_manager()->Show(nudge_data);

  // Update nudge prefs.
  pref_service->SetInteger(prefs::kMahiNudgeShownCount, shown_count + 1);
  pref_service->SetTime(prefs::kMahiNudgeLastShownTime, base::Time::Now());
}

}  // namespace ash
