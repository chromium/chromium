// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/camera/autozoom_nudge_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/camera/autozoom_controller_impl.h"
#include "base/json/values_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {
// Keys for autozoom nudge sub-preferences for shown count and last time shown.
constexpr char kShownCount[] = "shown_count";
constexpr char kLastTimeShown[] = "last_time_shown";
constexpr char kHadEnabled[] = "had_enabled";
constexpr char kAutozoomNudgeId[] = "AutozoomNudge";
constexpr int kNotificationLimit = 3;
constexpr base::TimeDelta kMinInterval = base::Days(1);
}  // namespace

AutozoomNudgeController::AutozoomNudgeController(
    AutozoomControllerImpl* autozoom_controller)
    : autozoom_controller_(autozoom_controller) {
  autozoom_controller_->AddObserver(this);

  if (base::FeatureList::IsEnabled(features::kAutozoomNudgeSessionReset))
    Shell::Get()->session_controller()->AddObserver(this);
}

AutozoomNudgeController::~AutozoomNudgeController() {
  autozoom_controller_->RemoveObserver(this);

  if (base::FeatureList::IsEnabled(features::kAutozoomNudgeSessionReset))
    Shell::Get()->session_controller()->RemoveObserver(this);
}

// static
void AutozoomNudgeController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kAutozoomNudges);
}

void AutozoomNudgeController::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  // Reset the nudge prefs so that the nudge can be shown again. This observer
  // callback is only registered and called when
  // features::kAutozoomNudgeSessionReset is enabled.
  ScopedDictPrefUpdate update(prefs, prefs::kAutozoomNudges);
  update->Set(kShownCount, 0);
  update->Set(kLastTimeShown, base::TimeToValue(base::Time()));
  update->Set(kHadEnabled, false);
}

base::Time AutozoomNudgeController::GetTime() {
  return base::Time::Now();
}

void AutozoomNudgeController::HandleNudgeShown() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!prefs)
    return;
  const int shown_count = GetShownCount(prefs);
  ScopedDictPrefUpdate update(prefs, prefs::kAutozoomNudges);
  update->Set(kShownCount, shown_count + 1);
  update->Set(kLastTimeShown, base::TimeToValue(GetTime()));
}

bool AutozoomNudgeController::GetHadEnabled(PrefService* prefs) {
  const base::Value::Dict& dictionary = prefs->GetDict(prefs::kAutozoomNudges);
  return dictionary.FindBool(kHadEnabled).value_or(false);
}

int AutozoomNudgeController::GetShownCount(PrefService* prefs) {
  const base::Value::Dict& dictionary = prefs->GetDict(prefs::kAutozoomNudges);
  return dictionary.FindInt(kShownCount).value_or(0);
}

base::Time AutozoomNudgeController::GetLastShownTime(PrefService* prefs) {
  const base::Value::Dict& dictionary = prefs->GetDict(prefs::kAutozoomNudges);
  std::optional<base::Time> last_shown_time =
      base::ValueToTime(dictionary.Find(kLastTimeShown));
  return last_shown_time.value_or(base::Time());
}

bool AutozoomNudgeController::ShouldShowNudge(PrefService* prefs) {
  if (!prefs)
    return false;

  bool had_enabled = GetHadEnabled(prefs);
  // We should not show more nudge after user had enabled autozoom before.
  if (had_enabled)
    return false;

  int nudge_shown_count = GetShownCount(prefs);
  // We should not show more nudges after hitting the limit.
  if (nudge_shown_count >= kNotificationLimit)
    return false;

  base::Time last_shown_time = GetLastShownTime(prefs);
  // If the nudge has yet to be shown, we should return true.
  if (last_shown_time.is_null())
    return true;

  // We should show the nudge if enough time has passed since the nudge was last
  // shown.
  return (base::Time::Now() - last_shown_time) > kMinInterval;
}

void AutozoomNudgeController::OnAutozoomControlEnabledChanged(bool enabled) {
  if (!enabled)
    return;

  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!ShouldShowNudge(prefs)) {
    return;
  }

  AnchoredNudgeData nudge_data(
      kAutozoomNudgeId, NudgeCatalogName::kAutozoom,
      l10n_util::GetStringUTF16(
          IDS_ASH_STATUS_TRAY_AUTOZOOM_EDUCATIONAL_NUDGE_TEXT));
  AnchoredNudgeManager::Get()->Show(nudge_data);
  HandleNudgeShown();
}

void AutozoomNudgeController::OnAutozoomStateChanged(
    cros::mojom::CameraAutoFramingState state) {
  if (state != cros::mojom::CameraAutoFramingState::OFF) {
    PrefService* prefs =
        Shell::Get()->session_controller()->GetLastActiveUserPrefService();
    if (!prefs)
      return;
    ScopedDictPrefUpdate update(prefs, prefs::kAutozoomNudges);
    update->Set(kHadEnabled, true);
  }
}

}  // namespace ash
