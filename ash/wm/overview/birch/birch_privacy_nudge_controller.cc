// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/birch/birch_privacy_nudge_controller.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/toast/anchored_nudge_manager_impl.h"
#include "base/command_line.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Maximum number of times to show the nudge.
constexpr int kMaxShownCount = 3;

// Minimum time between shows.
constexpr base::TimeDelta kTimeBetweenShown = base::Hours(24);

PrefService* GetPrefService() {
  return Shell::Get()->session_controller()->GetActivePrefService();
}

}  // namespace

BirchPrivacyNudgeController::BirchPrivacyNudgeController() = default;

BirchPrivacyNudgeController::~BirchPrivacyNudgeController() = default;

// static
void BirchPrivacyNudgeController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kBirchContextMenuShown, false);
  registry->RegisterIntegerPref(prefs::kBirchPrivacyNudgeShownCount, 0);
  registry->RegisterTimePref(prefs::kBirchPrivacyNudgeLastShownTime,
                             base::Time());
}

// static
void BirchPrivacyNudgeController::DidShowContextMenu() {
  GetPrefService()->SetBoolean(prefs::kBirchContextMenuShown, true);
}

void BirchPrivacyNudgeController::MaybeShowNudge(views::View* anchor_view) {
  auto* prefs = GetPrefService();

  // Don't show nudge if the user has already opened the context menu.
  if (prefs->GetBoolean(prefs::kBirchContextMenuShown)) {
    return;
  }

  // Nudge has been shown three times. No need to educate anymore.
  const int shown_count =
      prefs->GetInteger(prefs::kBirchPrivacyNudgeShownCount);
  if (shown_count >= kMaxShownCount) {
    return;
  }

  // Don't show nudge if it was shown within the last 24 hours.
  const base::Time last_shown =
      prefs->GetTime(prefs::kBirchPrivacyNudgeLastShownTime);
  if (base::Time::Now() - last_shown < kTimeBetweenShown) {
    return;
  }

  AnchoredNudgeData nudge_data(
      "BirchPrivacyId", NudgeCatalogName::kBirchPrivacy,
      l10n_util::GetStringUTF16(IDS_ASH_BIRCH_PRIVACY_NUDGE), anchor_view);
  nudge_data.arrow = views::BubbleBorder::BOTTOM_LEFT;

  Shell::Get()->anchored_nudge_manager()->Show(nudge_data);

  // Update nudge prefs.
  prefs->SetInteger(prefs::kBirchPrivacyNudgeShownCount, shown_count + 1);
  prefs->SetTime(prefs::kBirchPrivacyNudgeLastShownTime, base::Time::Now());
}

}  // namespace ash
