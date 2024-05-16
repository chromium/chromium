// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONTROLLER_H_
#define ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_nudge_constants.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "ui/base/clipboard/clipboard_observer.h"

class PrefRegistrySimple;
class ClipboardHistoryItem;

namespace base {
class Clock;
}  // namespace base

namespace crosapi::mojom {
enum class ClipboardHistoryControllerShowSource;
}  // namespace crosapi::mojom

namespace ui {
class ClipboardMonitor;
}  // namespace ui

namespace ash {

class ASH_EXPORT ClipboardNudgeController
    : public ClipboardHistory::Observer,
      public ui::ClipboardObserver,
      public ClipboardHistoryController::Observer {
 public:
  // Describes the clipboard history feature onboarding state. A user must
  // perform copy, paste, copy and paste in sequence to activate onboarding
  // nudges.
  enum class OnboardingState {
    kInit = 0,
    kFirstCopy = 1,
    kFirstPaste = 2,
    kSecondCopy = 3,
  };

  explicit ClipboardNudgeController(ClipboardHistory* clipboard_history);
  ClipboardNudgeController(const ClipboardNudgeController&) = delete;
  ClipboardNudgeController& operator=(const ClipboardNudgeController&) = delete;
  ~ClipboardNudgeController() override;

  // Registers profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // ui::ClipboardHistory::Observer:
  void OnClipboardHistoryItemAdded(const ClipboardHistoryItem& item,
                                   bool is_duplicate = false) override;

  // ui::ClipboardObserver:
  void OnClipboardDataRead() override;

  // ClipboardHistoryController::Observer:
  void OnClipboardHistoryMenuShown(
      crosapi::mojom::ClipboardHistoryControllerShowSource show_source)
      override;
  void OnClipboardHistoryPasted() override;

  // Returns the time in this session that any nudge was last shown, or
  // `std::nullopt` if a nudge has not been shown.
  std::optional<base::Time> GetNudgeLastTimeShown() const;

  // Increments the screenshot notification shown count.
  void MarkScreenshotNotificationShown();

  // Shows the nudge widget.
  void ShowNudge(ClipboardNudgeType nudge_type);

  // Test methods for overriding and resetting the clock used by GetTime.
  void OverrideClockForTesting(base::Clock* test_clock);
  void ClearClockOverrideForTesting();

  OnboardingState onboarding_state_for_testing() const {
    return onboarding_state_;
  }

 private:
  // An internal class to track the nudge of the specified type then record the
  // time deltas between showing a nudge and using clipboard history (e.g.
  // showing the standalone clipboard history menu or pasting from the
  // standalone menu). This allows us to understand the conversion rate of
  // showing a nudge to the clipboard history feature usage.
  class NudgeTimeDeltaRecorder {
   public:
    constexpr explicit NudgeTimeDeltaRecorder(ClipboardNudgeType nudge_type);
    NudgeTimeDeltaRecorder(const NudgeTimeDeltaRecorder&) = delete;
    NudgeTimeDeltaRecorder& operator=(const NudgeTimeDeltaRecorder&) = delete;
    ~NudgeTimeDeltaRecorder();

    // Called when the nudge of `nudge_type` shows.
    void OnNudgeShown();

    // Called when the clipboard history data is pasted. Maybe records the time
    // delta since `nudge_shown_time_`.
    void OnClipboardHistoryPasted();

    // Called when the clipboard history menu shows. Maybe records the time
    // delta since `nudge_shown_time_`.
    void OnClipboardHistoryMenuShown();

    // Returns the time in this session that the associated nudge was last
    // shown, or a null value if the associated nudge was never shown.
    const base::Time& nudge_shown_time() const { return nudge_shown_time_; }

   private:
    // Resets the tracking on the most recent nudge shown, if any. If the
    // previous nudge shown does not lead to a type of clipboard history usage
    // (e.g. showing the standalone menu or pasting from the standalone menu),
    // records the corresponding metric.
    void Reset();

    // Gets the time delta since `nudge_shown_time_`.
    base::TimeDelta GetTimeSinceNudgeShown() const;

    // Returns true if the time delta between `nudge_shown_time_` and the
    // clipboard history data paste time should be recorded.
    bool ShouldRecordClipboardHistoryPasteTimeDelta() const;

    // Returns true if the time delta between `nudge_shown_time_` and the
    // clipboard history menu shown time should be recorded.
    bool ShouldRecordMenuOpenTimeDelta() const;

    // The type of the nudge that this class is tracking.
    const ClipboardNudgeType nudge_type_;

    // Indicates the time stamp when the nudge specified by `nudge_type_` showed
    // most recently. NOTE: It is different from the last shown time cached in
    // the pref data which is the last nudge shown time across user sessions. In
    // contrast, `nudge_shown_time_` is the last shown time of the nudge of
    // `nudge_type_` in the current user session.
    base::Time nudge_shown_time_;

    // True if pasting clipboard history data has ever been recorded since the
    // last nudge shown.
    bool has_recorded_paste_ = false;

    // True if showing the clipboard history menu has ever been recorded since
    // the last nudge shown.
    bool has_recorded_menu_shown_ = false;
  };

  // Caches the onboarding state.
  // TODO(http://b/284368255): move this data member to a separate class.
  OnboardingState onboarding_state_ = OnboardingState::kInit;

  // The timestamp of the most recent paste.
  // TODO(http://b/284368255): move this data member to a separate class.
  base::Time last_paste_timestamp_;

  // The current nudge type being shown from ShowNudge().
  ClipboardNudgeType current_nudge_type_;

  // Nudge time delta recorders ------------------------------------------------

  // Records time deltas for the onboarding nudge.
  NudgeTimeDeltaRecorder onboarding_nudge_recorder_{
      ClipboardNudgeType::kOnboardingNudge};

  // Records time deltas for the zero state nudge.
  NudgeTimeDeltaRecorder zero_state_nudge_recorder_{
      ClipboardNudgeType::kZeroStateNudge};

  // Records time deltas for the screenshot notification nudge.
  NudgeTimeDeltaRecorder screenshot_nudge_recorder_{
      ClipboardNudgeType::kScreenshotNotificationNudge};

  // Records time deltas for the duplicate copy nudge. Used only when the
  // clipboard history refresh feature is enabled.
  NudgeTimeDeltaRecorder duplicate_copy_nudge_recorder_{
      ClipboardNudgeType::kDuplicateCopyNudge};

  // Observations --------------------------------------------------------------

  // The observation on the clipboard history.
  base::ScopedObservation<ClipboardHistory, ClipboardHistory::Observer>
      clipboard_history_observation_{this};

  // The observation on the clipboard history controller.
  base::ScopedObservation<ClipboardHistoryController,
                          ClipboardHistoryController::Observer>
      clipboard_history_controller_observation_{this};

  // The observation on the clipboard monitor.
  base::ScopedObservation<ui::ClipboardMonitor, ui::ClipboardObserver>
      clipboard_monitor_observation_{this};
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONTROLLER_H_
