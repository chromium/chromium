// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_education_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

// A nudge/tutorial will not be shown if it already been shown 3 times, or if 24
// hours have not yet passed since it was last shown.
constexpr int kNudgeMaxShownCount = 3;
constexpr base::TimeDelta kNudgeTimeBetweenShown = base::Hours(24);

constexpr char kCaptureModeNudgeId[] = "kCaptureModeNudge";

// Nudge styling values.
constexpr int kShortcutIconSize = 60;

// Clock that can be overridden for testing.
base::Clock* g_clock_override = nullptr;

PrefService* GetPrefService() {
  return Shell::Get()->session_controller()->GetActivePrefService();
}

base::Time GetTime() {
  return g_clock_override ? g_clock_override->Now() : base::Time::Now();
}

}  // namespace

CaptureModeEducationController::CaptureModeEducationController() = default;

CaptureModeEducationController::~CaptureModeEducationController() = default;

// static
void CaptureModeEducationController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterIntegerPref(prefs::kCaptureModeEducationShownCount, 0);
  registry->RegisterTimePref(prefs::kCaptureModeEducationLastShown,
                             base::Time());
}

// static
bool CaptureModeEducationController::IsArm1ShortcutNudgeEnabled() {
  return features::IsCaptureModeEducationEnabled() &&
         features::kCaptureModeEducationParam.Get() ==
             features::CaptureModeEducationParam::kShortcutNudge;
}

// static
bool CaptureModeEducationController::IsArm2ShortcutTutorialEnabled() {
  return features::IsCaptureModeEducationEnabled() &&
         features::kCaptureModeEducationParam.Get() ==
             features::CaptureModeEducationParam::kShortcutTutorial;
}

// static
bool CaptureModeEducationController::IsArm3SettingsNudgeEnabled() {
  return features::IsCaptureModeEducationEnabled() &&
         features::kCaptureModeEducationParam.Get() ==
             features::CaptureModeEducationParam::kSettingsNudge;
}

void CaptureModeEducationController::MaybeShowEducation() {
  auto* pref_service = GetPrefService();
  CHECK(pref_service);

  // We don't want to show the nudge if the user is not signed in yet or is on
  // the lock screen.
  if (Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    return;
  }

  if (!skip_prefs_for_test_) {
    const int shown_count =
        pref_service->GetInteger(prefs::kCaptureModeEducationShownCount);
    const base::Time last_shown_time =
        pref_service->GetTime(prefs::kCaptureModeEducationLastShown);

    // Do not show the nudge more than three times, or if it has already been
    // shown in the past 24 hours.
    const base::Time now = GetTime();
    if ((shown_count >= kNudgeMaxShownCount) ||
        ((now - last_shown_time) < kNudgeTimeBetweenShown)) {
      return;
    }

    // Update the preferences since a form of education must have been shown, as
    // `this` is only created if the feature flag is enabled.
    pref_service->SetInteger(prefs::kCaptureModeEducationShownCount,
                             shown_count + 1);
    pref_service->SetTime(prefs::kCaptureModeEducationLastShown, now);
  }

  if (IsArm1ShortcutNudgeEnabled()) {
    ShowShortcutNudge();
  }
}

// static
void CaptureModeEducationController::SetOverrideClockForTesting(
    base::Clock* test_clock) {
  g_clock_override = test_clock;
}

void CaptureModeEducationController::ShowShortcutNudge() {
  // Close the nudge if it already exists.
  auto* nudge_manager = AnchoredNudgeManager::Get();
  nudge_manager->Cancel(kCaptureModeNudgeId);

  AnchoredNudgeData nudge_data(
      kCaptureModeNudgeId, NudgeCatalogName::kCaptureModeEducationShortcutNudge,
      l10n_util::GetStringUTF16(IDS_ASH_SCREEN_CAPTURE_EDUCATION_NUDGE_LABEL));

  nudge_data.image_model = ui::ImageModel::FromVectorIcon(
      kCaptureModeIcon, kColorAshIconColorPrimary, kShortcutIconSize);
  nudge_data.keyboard_codes = {ui::VKEY_CONTROL, ui::VKEY_SHIFT,
                               ui::VKEY_MEDIA_LAUNCH_APP1};

  // TODO(b/302368860): Add a new view to display keyboard shortcuts in the same
  // style as the launcher and the new keyboard shortcut app.
  nudge_manager->Show(nudge_data);
}

}  // namespace ash
