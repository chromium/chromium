// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_nudge_controller.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/clipboard_nudge.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/clipboard/clipboard_monitor.h"

namespace ash {
namespace {

// Keys for tooltip sub-preferences for shown count and last time shown.
constexpr char kShownCount[] = "shown_count";
constexpr char kLastTimeShown[] = "last_time_shown";

// The maximum number of 1 second buckets used to record the time between
// showing the nudge and recording the feature being opened/used.
constexpr int kMaxSeconds = 61;

// Clock that can be overridden for testing.
base::Clock* g_clock_override = nullptr;

base::Time GetTime() {
  if (g_clock_override)
    return g_clock_override->Now();
  return base::Time::Now();
}

bool LogFeatureOpenTime(
    const ClipboardNudgeController::TimeMetricHelper& metric_show_time,
    const std::string& open_histogram) {
  if (!metric_show_time.ShouldLogFeatureOpenTime())
    return false;
  base::TimeDelta time_since_shown =
      metric_show_time.GetTimeSinceShown(GetTime());
  // Tracks the amount of time between showing the user a nudge and
  // the user opening the ClipboardHistory menu.
  base::UmaHistogramExactLinear(open_histogram, time_since_shown.InSeconds(),
                                kMaxSeconds);
  return true;
}

bool LogFeatureUsedTime(
    const ClipboardNudgeController::TimeMetricHelper& metric_show_time,
    const std::string& paste_histogram) {
  if (!metric_show_time.ShouldLogFeatureUsedTime())
    return false;
  base::TimeDelta time_since_shown =
      metric_show_time.GetTimeSinceShown(GetTime());
  // Tracks the amount of time between showing the user a nudge and
  // the user opening the ClipboardHistory menu.
  base::UmaHistogramExactLinear(paste_histogram, time_since_shown.InSeconds(),
                                kMaxSeconds);
  return true;
}
}  // namespace

ClipboardNudgeController::ClipboardNudgeController(
    ClipboardHistory* clipboard_history,
    ClipboardHistoryControllerImpl* clipboard_history_controller)
    : clipboard_history_(clipboard_history),
      clipboard_history_controller_(clipboard_history_controller) {
  clipboard_history_->AddObserver(this);
  clipboard_history_controller_->AddObserver(this);
  ui::ClipboardMonitor::GetInstance()->AddObserver(this);
  if (features::IsClipboardHistoryNudgeSessionResetEnabled())
    Shell::Get()->session_controller()->AddObserver(this);
}

ClipboardNudgeController::~ClipboardNudgeController() {
  clipboard_history_->RemoveObserver(this);
  clipboard_history_controller_->RemoveObserver(this);
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
  if (features::IsClipboardHistoryNudgeSessionResetEnabled())
    Shell::Get()->session_controller()->RemoveObserver(this);
}

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
  if (!ShouldShowNudge(prefs))
    return;

  switch (clipboard_state_) {
    case ClipboardState::kInit:
      clipboard_state_ = ClipboardState::kFirstCopy;
      return;
    case ClipboardState::kFirstPaste:
      clipboard_state_ = ClipboardState::kSecondCopy;
      return;
    case ClipboardState::kFirstCopy:
    case ClipboardState::kSecondCopy:
    case ClipboardState::kShouldShowNudge:
      return;
  }
}

void ClipboardNudgeController::MarkScreenshotNotificationShown() {
  base::UmaHistogramBoolean(kScreenshotNotification_ShowCount, true);
  if (screenshot_notification_last_shown_time_.ShouldLogFeatureOpenTime()) {
    base::UmaHistogramExactLinear(kScreenshotNotification_OpenTime, kMaxSeconds,
                                  kMaxSeconds);
  }
  if (screenshot_notification_last_shown_time_.ShouldLogFeatureUsedTime()) {
    base::UmaHistogramExactLinear(kScreenshotNotification_PasteTime,
                                  kMaxSeconds, kMaxSeconds);
  }
  screenshot_notification_last_shown_time_.ResetTime();
}

void ClipboardNudgeController::OnClipboardDataRead() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!clipboard_history_util::IsEnabledInCurrentMode() || !prefs ||
      !ShouldShowNudge(prefs)) {
    return;
  }

  switch (clipboard_state_) {
    case ClipboardState::kFirstCopy:
      clipboard_state_ = ClipboardState::kFirstPaste;
      last_paste_timestamp_ = GetTime();
      return;
    case ClipboardState::kFirstPaste:
      // Subsequent pastes should reset the timestamp.
      last_paste_timestamp_ = GetTime();
      return;
    case ClipboardState::kSecondCopy:
      if (GetTime() - last_paste_timestamp_ < kMaxTimeBetweenPaste) {
        ShowNudge(ClipboardNudgeType::kOnboardingNudge);
        HandleNudgeShown();
      } else {
        // ClipboardState should be reset to kFirstPaste when timed out.
        clipboard_state_ = ClipboardState::kFirstPaste;
        last_paste_timestamp_ = GetTime();
      }
      return;
    case ClipboardState::kInit:
    case ClipboardState::kShouldShowNudge:
      return;
  }
}

void ClipboardNudgeController::OnActiveUserPrefServiceChanged(
    PrefService* prefs) {
  // Reset the nudge prefs so that the nudge can be shown again.
  ScopedDictPrefUpdate update(prefs, prefs::kMultipasteNudges);
  update->Set(kShownCount, 0);
  update->Set(kLastTimeShown, base::TimeToValue(base::Time()));
}

void ClipboardNudgeController::ShowNudge(ClipboardNudgeType nudge_type) {
  current_nudge_type_ = nudge_type;
  SystemNudgeController::ShowNudge();

  // Tracks the number of times the ClipboardHistory nudge is shown.
  // This allows us to understand the conversion rate of showing a nudge to
  // a user opening and then using the clipboard history feature.
  switch (nudge_type) {
    case ClipboardNudgeType::kOnboardingNudge:
      if (last_shown_time_.ShouldLogFeatureOpenTime()) {
        base::UmaHistogramExactLinear(kOnboardingNudge_OpenTime, kMaxSeconds,
                                      kMaxSeconds);
      }
      if (last_shown_time_.ShouldLogFeatureUsedTime()) {
        base::UmaHistogramExactLinear(kOnboardingNudge_PasteTime, kMaxSeconds,
                                      kMaxSeconds);
      }
      last_shown_time_.ResetTime();
      base::UmaHistogramBoolean(kOnboardingNudge_ShowCount, true);
      break;
    case ClipboardNudgeType::kZeroStateNudge:
      if (zero_state_last_shown_time_.ShouldLogFeatureOpenTime()) {
        base::UmaHistogramExactLinear(kZeroStateNudge_OpenTime, kMaxSeconds,
                                      kMaxSeconds);
      }
      if (zero_state_last_shown_time_.ShouldLogFeatureUsedTime()) {
        base::UmaHistogramExactLinear(kZeroStateNudge_PasteTime, kMaxSeconds,
                                      kMaxSeconds);
      }
      zero_state_last_shown_time_.ResetTime();
      base::UmaHistogramBoolean(kZeroStateNudge_ShowCount, true);
      break;
    default:
      NOTREACHED();
  }
}

void ClipboardNudgeController::HandleNudgeShown() {
  clipboard_state_ = ClipboardState::kInit;
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!prefs)
    return;
  const int shown_count = GetShownCount(prefs);
  ScopedDictPrefUpdate update(prefs, prefs::kMultipasteNudges);
  update->Set(kShownCount, shown_count + 1);
  update->Set(kLastTimeShown, base::TimeToValue(GetTime()));
}

void ClipboardNudgeController::OnClipboardHistoryMenuShown() {
  if (LogFeatureOpenTime(last_shown_time_, kOnboardingNudge_OpenTime))
    last_shown_time_.set_was_logged_as_opened();
  if (LogFeatureOpenTime(zero_state_last_shown_time_, kZeroStateNudge_OpenTime))
    zero_state_last_shown_time_.set_was_logged_as_opened();
  if (LogFeatureOpenTime(screenshot_notification_last_shown_time_,
                         kScreenshotNotification_OpenTime)) {
    screenshot_notification_last_shown_time_.set_was_logged_as_opened();
  }
}

void ClipboardNudgeController::OnClipboardHistoryPasted() {
  if (LogFeatureUsedTime(last_shown_time_, kOnboardingNudge_PasteTime))
    last_shown_time_.set_was_logged_as_used();
  if (LogFeatureUsedTime(zero_state_last_shown_time_,
                         kZeroStateNudge_PasteTime)) {
    zero_state_last_shown_time_.set_was_logged_as_used();
  }
  if (LogFeatureUsedTime(screenshot_notification_last_shown_time_,
                         kScreenshotNotification_PasteTime)) {
    screenshot_notification_last_shown_time_.set_was_logged_as_used();
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

const ClipboardState& ClipboardNudgeController::GetClipboardStateForTesting() {
  return clipboard_state_;
}

std::unique_ptr<SystemNudge> ClipboardNudgeController::CreateSystemNudge() {
  return std::make_unique<ClipboardNudge>(current_nudge_type_);
}

int ClipboardNudgeController::GetShownCount(PrefService* prefs) {
  const base::Value::Dict& dictionary =
      prefs->GetDict(prefs::kMultipasteNudges);
  return dictionary.FindInt(kShownCount).value_or(0);
}

base::Time ClipboardNudgeController::GetLastShownTime(PrefService* prefs) {
  const base::Value::Dict& dictionary =
      prefs->GetDict(prefs::kMultipasteNudges);
  absl::optional<base::Time> last_shown_time =
      base::ValueToTime(dictionary.Find(kLastTimeShown));
  return last_shown_time.value_or(base::Time());
}

bool ClipboardNudgeController::ShouldShowNudge(PrefService* prefs) {
  if (!prefs)
    return false;
  int nudge_shown_count = GetShownCount(prefs);
  base::Time last_shown_time = GetLastShownTime(prefs);
  // We should not show more nudges after hitting the limit.
  if (nudge_shown_count >= kNotificationLimit)
    return false;
  // If the nudge has yet to be shown, we should return true.
  if (last_shown_time.is_null())
    return true;

  // We should show the nudge if enough time has passed since the nudge was last
  // shown.
  return base::Time::Now() - last_shown_time > kMinInterval;
}

base::Time ClipboardNudgeController::GetTime() {
  if (g_clock_override)
    return g_clock_override->Now();
  return base::Time::Now();
}

void ClipboardNudgeController::TimeMetricHelper::ResetTime() {
  last_shown_time_ =
      g_clock_override ? g_clock_override->Now() : base::Time::Now();
  was_logged_as_opened_ = false;
  was_logged_as_used_ = false;
}

bool ClipboardNudgeController::TimeMetricHelper::ShouldLogFeatureUsedTime()
    const {
  return !last_shown_time_.is_null() && !was_logged_as_used_;
}

bool ClipboardNudgeController::TimeMetricHelper::ShouldLogFeatureOpenTime()
    const {
  return !last_shown_time_.is_null() && !was_logged_as_opened_;
}

base::TimeDelta ClipboardNudgeController::TimeMetricHelper::GetTimeSinceShown(
    base::Time current_time) const {
  return current_time - last_shown_time_;
}

}  // namespace ash
