// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_nudge_controller.h"

#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/clipboard_nudge_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/notifier_catalogs.h"
#include "ash/public/cpp/resources/grit/ash_public_unscaled_resources.h"
#include "ash/public/cpp/system/anchored_nudge_data.h"
#include "ash/public/cpp/system/anchored_nudge_manager.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "ui/base/clipboard/clipboard_monitor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace ash {
namespace {

// Clock that can be overridden for testing.
base::Clock* g_clock_override = nullptr;

// Capped nudge constants ------------------------------------------------------
// The pref keys used by the capped nudges (i.e. the nudges that have a
// limited number of times they can be shown to a user). The associated pref
// data are recorded across user sessions.

// The last time shown, shared by all capped nudges. Updated when a nudge shows.
constexpr char kCappedNudgeLastTimeShown[] = "last_time_shown";

// The shown count of duplicate copy nudges.
constexpr char kShownCountDuplicateCopyNudge[] =
    "shown_count_duplicate_copy_nudge";

// The shown count of onboarding nudges.
constexpr char kShownCountOnboardingNudge[] = "shown_count";

// Constants -------------------------------------------------------------------

// The id used for clipboard nudges.
constexpr char kClipboardNudgeId[] = "ClipboardContextualNudge";

// The maximum number of 1 second buckets, used to record the time delta between
// when a nudge shows and when the clipboard history menu shows or clipboard
// history data is pasted.
constexpr int kMaxSeconds = 61;

// Helpers ---------------------------------------------------------------------

int GetBodyTextStringId(ClipboardNudgeType nudge_type) {
  switch (nudge_type) {
    case ClipboardNudgeType::kDuplicateCopyNudge:
      return IDS_ASH_MULTIPASTE_DUPLICATE_COPY_NUDGE;
    case ClipboardNudgeType::kOnboardingNudge:
      return IDS_ASH_MULTIPASTE_CONTEXTUAL_NUDGE;
    case ClipboardNudgeType::kScreenshotNotificationNudge:
      return IDS_ASH_MULTIPASTE_SCREENSHOT_NOTIFICATION_NUDGE;
    case ClipboardNudgeType::kZeroStateNudge:
      return IDS_ASH_MULTIPASTE_ZERO_STATE_CONTEXTUAL_NUDGE;
  }
}

NudgeCatalogName GetCatalogName(ClipboardNudgeType type) {
  switch (type) {
    case kOnboardingNudge:
      return NudgeCatalogName::kClipboardHistoryOnboarding;
    case kZeroStateNudge:
      return NudgeCatalogName::kClipboardHistoryZeroState;
    case kScreenshotNotificationNudge:
      NOTREACHED();
    case kDuplicateCopyNudge:
      return NudgeCatalogName::kClipboardHistoryDuplicateCopy;
  }
  NOTREACHED();
}

ui::ImageModel GetImage(ClipboardNudgeType type) {
  switch (type) {
    case kDuplicateCopyNudge:
    case kOnboardingNudge:
      return ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_CLIPBOARD_NUDGE_COPIED_IMAGE);
    case kScreenshotNotificationNudge:
    case kZeroStateNudge:
      return ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
          IDR_CLIPBOARD_NUDGE_SELECT_IMAGE);
  }
}

base::Time GetTime() {
  return g_clock_override ? g_clock_override->Now() : base::Time::Now();
}

// Capped nudge helpers --------------------------------------------------------

// Returns true if `type` indicates a capped nudge.
bool IsCappedNudge(ClipboardNudgeType type) {
  switch (type) {
    case kOnboardingNudge:
    case kDuplicateCopyNudge:
      return true;
    case kScreenshotNotificationNudge:
    case kZeroStateNudge:
      return false;
  }
}

// Gets the pref key to the shown count of the specified capped nudge.
const char* GetCappedNudgeShownCountPrefKey(ClipboardNudgeType type) {
  CHECK(IsCappedNudge(type));
  switch (type) {
    case kOnboardingNudge:
      return kShownCountOnboardingNudge;
    case kDuplicateCopyNudge:
      return kShownCountDuplicateCopyNudge;
    case kScreenshotNotificationNudge:
    case kZeroStateNudge:
      NOTREACHED();
  }
}

// Gets the number of times the specified capped nudge has shown across user
// sessions.
int GetCappedNudgeShownCount(const PrefService& prefs,
                             ClipboardNudgeType type) {
  return prefs.GetDict(prefs::kMultipasteNudges)
      .FindInt(GetCappedNudgeShownCountPrefKey(type))
      .value_or(0);
}

// Gets the last time the capped nudge was shown across user sessions.
base::Time GetCappedNudgeLastShownTime(const PrefService& prefs) {
  const std::optional<base::Time> last_shown_time = base::ValueToTime(
      prefs.GetDict(prefs::kMultipasteNudges).Find(kCappedNudgeLastTimeShown));
  return last_shown_time.value_or(base::Time());
}

// Checks if a capped nudge of the specified `type` can be shown. Returns true
// if:
// 1. The specified nudge's shown count is below the threshold; AND
// 2. Enough time has elapsed since the last capped nudge, if any, was shown.
bool ShouldShowCappedNudge(const PrefService& prefs, ClipboardNudgeType type) {
  // We should not show more nudges after hitting the limit.
  if (GetCappedNudgeShownCount(prefs, type) >= kCappedNudgeShownLimit) {
    return false;
  }

  // Returns true if:
  // 1. No capped nudge has been shown; OR
  // 2. Enough time has elapsed since the last capped nudge was shown.
  const base::Time last_shown_time = GetCappedNudgeLastShownTime(prefs);
  return last_shown_time.is_null() ||
         GetTime() - last_shown_time > kCappedNudgeMinInterval;
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
  const PrefService* const prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  if (!prefs) {
    return;
  }

  if (ShouldShowCappedNudge(*prefs, ClipboardNudgeType::kOnboardingNudge)) {
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
  }

  if (chromeos::features::IsClipboardHistoryRefreshEnabled() && is_duplicate &&
      ShouldShowCappedNudge(*prefs, ClipboardNudgeType::kDuplicateCopyNudge)) {
    ShowNudge(ClipboardNudgeType::kDuplicateCopyNudge);
  }
}

std::optional<base::Time> ClipboardNudgeController::GetNudgeLastTimeShown()
    const {
  const base::Time& nudge_last_time_shown =
      base::ranges::max(
          {&duplicate_copy_nudge_recorder_, &onboarding_nudge_recorder_,
           &screenshot_nudge_recorder_, &zero_state_nudge_recorder_},
          /*comp=*/{}, /*proj=*/&NudgeTimeDeltaRecorder::nudge_shown_time)
          ->nudge_shown_time();

  return nudge_last_time_shown.is_null()
             ? std::nullopt
             : std::make_optional(nudge_last_time_shown);
}

void ClipboardNudgeController::MarkScreenshotNotificationShown() {
  base::UmaHistogramBoolean(kClipboardHistoryScreenshotNotificationShowCount,
                            true);
  screenshot_nudge_recorder_.OnNudgeShown();
}

void ClipboardNudgeController::OnClipboardDataRead() {
  if (const PrefService* const prefs =
          Shell::Get()->session_controller()->GetLastActiveUserPrefService();
      clipboard_history_util::IsEnabledInCurrentMode() && prefs &&
      ShouldShowCappedNudge(*prefs, ClipboardNudgeType::kOnboardingNudge)) {
    switch (onboarding_state_) {
      case OnboardingState::kInit:
        return;
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
    }
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

  AnchoredNudgeManager::Get()->MaybeRecordNudgeAction(
      NudgeCatalogName::kClipboardHistoryOnboarding);
  AnchoredNudgeManager::Get()->MaybeRecordNudgeAction(
      NudgeCatalogName::kClipboardHistoryZeroState);

  if (chromeos::features::IsClipboardHistoryRefreshEnabled()) {
    duplicate_copy_nudge_recorder_.OnClipboardHistoryMenuShown();
    AnchoredNudgeManager::Get()->MaybeRecordNudgeAction(
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

  const std::u16string shortcut_key =
      clipboard_history_util::GetShortcutKeyName();
  const std::u16string body_text = l10n_util::GetStringFUTF16(
      GetBodyTextStringId(current_nudge_type_), shortcut_key);

  AnchoredNudgeData nudge_data(kClipboardNudgeId,
                               GetCatalogName(current_nudge_type_), body_text);
  nudge_data.image_model = GetImage(current_nudge_type_);

  AnchoredNudgeManager::Get()->Show(nudge_data);

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
      NOTREACHED();
    case ClipboardNudgeType::kDuplicateCopyNudge:
      CHECK(chromeos::features::IsClipboardHistoryRefreshEnabled());
      duplicate_copy_nudge_recorder_.OnNudgeShown();
      base::UmaHistogramBoolean(kClipboardHistoryDuplicateCopyNudgeShowCount,
                                true);
      break;
  }

  // Reset `onboarding_state_`.
  onboarding_state_ = OnboardingState::kInit;

  if (PrefService* const prefs =
          Shell::Get()->session_controller()->GetLastActiveUserPrefService();
      prefs && IsCappedNudge(nudge_type)) {
    ScopedDictPrefUpdate update(prefs, prefs::kMultipasteNudges);
    update->Set(GetCappedNudgeShownCountPrefKey(nudge_type),
                GetCappedNudgeShownCount(*prefs, nudge_type) + 1);
    update->Set(kCappedNudgeLastTimeShown, base::TimeToValue(GetTime()));
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

}  // namespace ash
