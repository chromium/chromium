// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_nudge_controller.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/clipboard_nudge.h"
#include "ash/clipboard/clipboard_nudge_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/clipboard/clipboard_monitor.h"

namespace ash {
namespace {

// Keys to fetch the nudge shown count and last time shown which are recorded
// across user sessions. NOTE: The zero state nudge and the screenshot
// notification nudge are not recorded across user sessions.
constexpr char kShownCount[] = "shown_count";
constexpr char kLastTimeShown[] = "last_time_shown";

// The maximum number of 1 second buckets, used to record the time delta between
// when a nudge shows and when the clipboard history menu shows or clipboard
// history data is pasted.
constexpr int kMaxSeconds = 61;

// Clock that can be overridden for testing.
base::Clock* g_clock_override = nullptr;

base::Time GetTime() {
  return g_clock_override ? g_clock_override->Now() : base::Time::Now();
}

NudgeCatalogName GetCatalogName(ClipboardNudgeType type) {
  switch (type) {
    case kOnboardingNudge:
      return NudgeCatalogName::kClipboardHistoryOnboarding;
    case kZeroStateNudge:
      return NudgeCatalogName::kClipboardHistoryZeroState;
    case kScreenshotNotificationNudge:
      NOTREACHED();
      break;
    case kDuplicateCopyNudge:
      return NudgeCatalogName::kClipboardHistoryDuplicateCopy;
  }
  return NudgeCatalogName::kTestCatalogName;
}

// Gets the number of times the nudge has shown across user sessions.
int GetShownCount(PrefService* prefs) {
  return prefs->GetDict(prefs::kMultipasteNudges)
      .FindInt(kShownCount)
      .value_or(0);
}

// Gets the last time the nudge was shown across user sessions.
base::Time GetLastShownTime(PrefService* prefs) {
  const base::Value::Dict& dictionary =
      prefs->GetDict(prefs::kMultipasteNudges);
  absl::optional<base::Time> last_shown_time =
      base::ValueToTime(dictionary.Find(kLastTimeShown));
  return last_shown_time.value_or(base::Time());
}

// Checks whether another nudge can be shown. Returns true if:
// 1. The count of nudges shown is below the threshold; and
// 2. The time interval since the last nudge shown, if any, is long enough.
bool ShouldShowNudge(PrefService* prefs) {
  // We should not show more nudges after hitting the limit.
  if (!prefs || GetShownCount(prefs) >= kNotificationLimit) {
    return false;
  }

  // If the nudge has yet to be shown, we should return true.
  const base::Time last_shown_time = GetLastShownTime(prefs);
  if (last_shown_time.is_null()) {
    return true;
  }

  // Check whether enough time has passed since the nudge was last shown.
  return GetTime() - last_shown_time > kMinInterval;
}

}  // namespace

// ClipboardNudgeController::NudgeTimeDeltaRecorder ---------------------------

constexpr ClipboardNudgeController::NudgeTimeDeltaRecorder::
    NudgeTimeDeltaRecorder(ClipboardNudgeType nudge_type)
    : nudge_type_(nudge_type) {}

ClipboardNudgeController::NudgeTimeDeltaRecorder::~NudgeTimeDeltaRecorder() {
  Reset();
}

void ClipboardNudgeController::NudgeTimeDeltaRecorder::OnNudgeShown() {
  Reset();
  nudge_shown_time_ = GetTime();
}

void ClipboardNudgeController::NudgeTimeDeltaRecorder::
    OnClipboardHistoryPasted() {
  if (ShouldRecordClipboardHistoryPasteTimeDelta()) {
    base::UmaHistogramExactLinear(
        GetClipboardHistoryPasteTimeDeltaHistogram(nudge_type_),
        GetTimeSinceNudgeShown().InSeconds(), kMaxSeconds);
    has_recorded_paste_ = true;
  }
}

void ClipboardNudgeController::NudgeTimeDeltaRecorder::
    OnClipboardHistoryMenuShown() {
  if (ShouldRecordMenuOpenTimeDelta()) {
    base::UmaHistogramExactLinear(GetMenuOpenTimeDeltaHistogram(nudge_type_),
                                  GetTimeSinceNudgeShown().InSeconds(),
                                  kMaxSeconds);
    has_recorded_menu_shown_ = true;
  }
}

void ClipboardNudgeController::NudgeTimeDeltaRecorder::Reset() {
  // Record `kMaxSeconds` if the standalone clipboard history menu has never
  // shown since the last nudge shown, if any.
  if (ShouldRecordMenuOpenTimeDelta()) {
    base::UmaHistogramExactLinear(GetMenuOpenTimeDeltaHistogram(nudge_type_),
                                  kMaxSeconds, kMaxSeconds);
  }

  // Record `kMaxSeconds` if the clipboard history data has never been pasted
  // since the last nudge shown, if any.
  if (ShouldRecordClipboardHistoryPasteTimeDelta()) {
    base::UmaHistogramExactLinear(
        GetClipboardHistoryPasteTimeDeltaHistogram(nudge_type_), kMaxSeconds,
        kMaxSeconds);
  }

  nudge_shown_time_ = base::Time();
  has_recorded_menu_shown_ = false;
  has_recorded_paste_ = false;
}

base::TimeDelta
ClipboardNudgeController::NudgeTimeDeltaRecorder::GetTimeSinceNudgeShown()
    const {
  CHECK(!nudge_shown_time_.is_null());
  return GetTime() - nudge_shown_time_;
}

bool ClipboardNudgeController::NudgeTimeDeltaRecorder::
    ShouldRecordClipboardHistoryPasteTimeDelta() const {
  return !nudge_shown_time_.is_null() && !has_recorded_paste_;
}

bool ClipboardNudgeController::NudgeTimeDeltaRecorder::
    ShouldRecordMenuOpenTimeDelta() const {
  return !nudge_shown_time_.is_null() && !has_recorded_menu_shown_;
}

// ClipboardNudgeController ----------------------------------------------------

ClipboardNudgeController::ClipboardNudgeController(
    ClipboardHistory* clipboard_history) {
  clipboard_history_observation_.Observe(clipboard_history);
  clipboard_history_controller_observation_.Observe(
      ClipboardHistoryController::Get());
  clipboard_monitor_observation_.Observe(ui::ClipboardMonitor::GetInstance());
}

ClipboardNudgeController::~ClipboardNudgeController() = default;

// static
void ClipboardNudgeController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kMultipasteNudges);
}

void ClipboardNudgeController::OnClipboardHistoryItemAdded(
    const ClipboardHistoryItem& item,
    bool is_duplicate) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!ShouldShowNudge(prefs)) {
    return;
  }

  switch (onboarding_state_) {
    case OnboardingState::kInit:
      onboarding_state_ = OnboardingState::kFirstCopy;
      break;
    case OnboardingState::kFirstPaste:
      onboarding_state_ = OnboardingState::kSecondCopy;
      break;
    case OnboardingState::kFirstCopy:
    case OnboardingState::kSecondCopy:
      break;
  }

  if (chromeos::features::IsClipboardHistoryRefreshEnabled() && is_duplicate) {
    ShowNudge(ClipboardNudgeType::kDuplicateCopyNudge);
  }
}

void ClipboardNudgeController::MarkScreenshotNotificationShown() {
  base::UmaHistogramBoolean(kClipboardHistoryScreenshotNotificationShowCount,
                            true);
  screenshot_nudge_recorder_.OnNudgeShown();
}

void ClipboardNudgeController::OnClipboardDataRead() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!clipboard_history_util::IsEnabledInCurrentMode() || !prefs ||
      !ShouldShowNudge(prefs)) {
    return;
  }

  switch (onboarding_state_) {
    case OnboardingState::kFirstCopy:
      onboarding_state_ = OnboardingState::kFirstPaste;
      last_paste_timestamp_ = GetTime();
      return;
    case OnboardingState::kFirstPaste:
      // Subsequent pastes should reset the timestamp.
      last_paste_timestamp_ = GetTime();
      return;
    case OnboardingState::kSecondCopy:
      if (GetTime() - last_paste_timestamp_ < kMaxTimeBetweenPaste) {
        ShowNudge(ClipboardNudgeType::kOnboardingNudge);
      } else {
        // Reset `onboarding_state_` to `kFirstPaste` when too much time has
        // elapsed since the last paste.
        onboarding_state_ = OnboardingState::kFirstPaste;
        last_paste_timestamp_ = GetTime();
      }
      return;
    case OnboardingState::kInit:
      return;
  }
}

void ClipboardNudgeController::OnClipboardHistoryMenuShown(
    crosapi::mojom::ClipboardHistoryControllerShowSource show_source) {
  // The clipboard history nudges specifically suggest trying the Search+V
  // shortcut. Opening the menu any other way should not count as the user
  // responding to the nudge.
  if (show_source !=
      crosapi::mojom::ClipboardHistoryControllerShowSource::kAccelerator) {
    return;
  }

  onboarding_nudge_recorder_.OnClipboardHistoryMenuShown();
  zero_state_nudge_recorder_.OnClipboardHistoryMenuShown();
  screenshot_nudge_recorder_.OnClipboardHistoryMenuShown();

  SystemNudgeController::MaybeRecordNudgeAction(
      NudgeCatalogName::kClipboardHistoryOnboarding);
  SystemNudgeController::MaybeRecordNudgeAction(
      NudgeCatalogName::kClipboardHistoryZeroState);

  if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    duplicate_copy_nudge_recorder_.OnClipboardHistoryMenuShown();
    SystemNudgeController::MaybeRecordNudgeAction(
        NudgeCatalogName::kClipboardHistoryDuplicateCopy);
  }
}

void ClipboardNudgeController::OnClipboardHistoryPasted() {
  onboarding_nudge_recorder_.OnClipboardHistoryPasted();
  zero_state_nudge_recorder_.OnClipboardHistoryPasted();
  screenshot_nudge_recorder_.OnClipboardHistoryPasted();

  if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    duplicate_copy_nudge_recorder_.OnClipboardHistoryPasted();
  }
}

void ClipboardNudgeController::ShowNudge(ClipboardNudgeType nudge_type) {
  current_nudge_type_ = nudge_type;
  SystemNudgeController::ShowNudge();

  switch (nudge_type) {
    case ClipboardNudgeType::kOnboardingNudge:
      onboarding_nudge_recorder_.OnNudgeShown();
      base::UmaHistogramBoolean(kClipboardHistoryOnboardingNudgeShowCount,
                                true);
      break;
    case ClipboardNudgeType::kZeroStateNudge:
      zero_state_nudge_recorder_.OnNudgeShown();
      base::UmaHistogramBoolean(kClipboardHistoryZeroStateNudgeShowCount, true);
      break;
    case ClipboardNudgeType::kScreenshotNotificationNudge:
      NOTREACHED_NORETURN();
    case ClipboardNudgeType::kDuplicateCopyNudge:
      CHECK(chromeos::features::IsClipboardHistoryRefreshEnabled());
      duplicate_copy_nudge_recorder_.OnNudgeShown();
      base::UmaHistogramBoolean(kClipboardHistoryDuplicateCopyNudgeShowCount,
                                true);
      break;
  }

  // Reset `onboarding_state_`.
  onboarding_state_ = OnboardingState::kInit;

  if (PrefService* prefs =
          Shell::Get()->session_controller()->GetLastActiveUserPrefService();
      prefs && nudge_type != ClipboardNudgeType::kZeroStateNudge) {
    const int shown_count = GetShownCount(prefs);
    ScopedDictPrefUpdate update(prefs, prefs::kMultipasteNudges);
    update->Set(kShownCount, shown_count + 1);
    update->Set(kLastTimeShown, base::TimeToValue(GetTime()));
  }
}

void ClipboardNudgeController::OverrideClockForTesting(
    base::Clock* test_clock) {
  DCHECK(!g_clock_override);
  g_clock_override = test_clock;
}

void ClipboardNudgeController::ClearClockOverrideForTesting() {
  DCHECK(g_clock_override);
  g_clock_override = nullptr;
}

std::unique_ptr<SystemNudge> ClipboardNudgeController::CreateSystemNudge() {
  return std::make_unique<ClipboardNudge>(current_nudge_type_,
                                          GetCatalogName(current_nudge_type_));
}

}  // namespace ash
