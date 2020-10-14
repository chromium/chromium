// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_nudge_controller.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_nudge.h"
#include "ash/clipboard/clipboard_nudge_constants.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/util/values/values_util.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/clipboard/clipboard_monitor.h"

namespace {

// Keys for tooltip sub-preferences for shown count and last time shown.
constexpr char kShownCount[] = "shown_count";
constexpr char kLastTimeShown[] = "last_time_shown";

}  // namespace

namespace ash {

ClipboardNudgeController::ClipboardNudgeController(
    ClipboardHistory* clipboard_history)
    : clipboard_history_(clipboard_history) {
  clipboard_history_->AddObserver(this);
  ui::ClipboardMonitor::GetInstance()->AddObserver(this);
}

ClipboardNudgeController::~ClipboardNudgeController() {
  clipboard_history_->RemoveObserver(this);
  ui::ClipboardMonitor::GetInstance()->RemoveObserver(this);
}

// static
void ClipboardNudgeController::RegisterProfilePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kMultipasteNudges);
}

void ClipboardNudgeController::OnClipboardHistoryItemAdded(
    const ClipboardHistoryItem& item) {
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

void ClipboardNudgeController::OnClipboardDataRead() {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!ShouldShowNudge(prefs))
    return;

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
        ShowNudge();
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

void ClipboardNudgeController::ShowNudge() {
  // Create and show the nudge.
  nudge_ = std::make_unique<ClipboardNudge>();

  // Start a timer to close the nudge after a set amount of time.
  hide_nudge_timer_.Start(FROM_HERE, kNudgeShowTime,
                          base::BindOnce(&ClipboardNudgeController::HideNudge,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void ClipboardNudgeController::HideNudge() {
  nudge_->Close();
  nudge_.reset();
}

void ClipboardNudgeController::HandleNudgeShown() {
  clipboard_state_ = ClipboardState::kInit;
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  const int shown_count = GetShownCount(prefs);
  DictionaryPrefUpdate update(prefs, prefs::kMultipasteNudges);
  update->SetIntPath(kShownCount, shown_count + 1);
  update->SetPath(kLastTimeShown, util::TimeToValue(GetTime()));
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

int ClipboardNudgeController::GetShownCount(PrefService* prefs) {
  base::Optional<int> nudge_shown_count =
      prefs->GetDictionary(prefs::kMultipasteNudges)->FindIntPath(kShownCount);
  return nudge_shown_count.value_or(0);
}

base::Time ClipboardNudgeController::GetLastShownTime(PrefService* prefs) {
  const base::Value* last_shown_time_val =
      prefs->GetDictionary(prefs::kMultipasteNudges)->FindPath(kLastTimeShown);
  if (!last_shown_time_val)
    return base::Time();
  base::Optional<base::Time> last_shown_time =
      *util::ValueToTime(last_shown_time_val);
  return last_shown_time.value_or(base::Time());
}

bool ClipboardNudgeController::ShouldShowNudge(PrefService* prefs) {
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

}  // namespace ash
