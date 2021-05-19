// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONTROLLER_H_
#define ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/clipboard/clipboard_history.h"
#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_nudge_constants.h"
#include "ash/public/cpp/clipboard_history_controller.h"
#include "ash/public/cpp/session/session_observer.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "chromeos/crosapi/mojom/clipboard_history.mojom.h"
#include "ui/base/clipboard/clipboard_observer.h"
#include "ui/compositor/layer_animation_observer.h"

class PrefService;
class PrefRegistrySimple;
class ClipboardHistoryItem;

namespace ash {
class ClipboardNudge;

// The clipboard contextual nudge will be shown after 4 user actions that must
// happen in sequence. The user must perform copy, paste, copy and paste in
// sequence to activate the nudge.
enum class ClipboardState {
  kInit = 0,
  kFirstCopy = 1,
  kFirstPaste = 2,
  kSecondCopy = 3,
  kShouldShowNudge = 4,
};

class ASH_EXPORT ClipboardNudgeController
    : public ClipboardHistory::Observer,
      public ui::ClipboardObserver,
      public SessionObserver,
      public ClipboardHistoryController::Observer {
 public:
  class TimeMetricHelper {
   public:
    TimeMetricHelper() = default;
    TimeMetricHelper(const TimeMetricHelper&) = delete;
    TimeMetricHelper& operator=(const TimeMetricHelper&) = delete;
    ~TimeMetricHelper() = default;

    bool ShouldLogFeatureUsedTime() const;
    bool ShouldLogFeatureOpenTime() const;
    base::TimeDelta GetTimeSinceShown(base::Time current_time) const;
    void ResetTime();
    void set_was_logged_as_used() { was_logged_as_used_ = true; }
    void set_was_logged_as_opened() { was_logged_as_opened_ = true; }

   private:
    base::Time last_shown_time_;
    bool was_logged_as_used_ = false;
    bool was_logged_as_opened_ = false;
  };
  ClipboardNudgeController(
      ClipboardHistory* clipboard_history,
      ClipboardHistoryControllerImpl* clipboard_history_controller);
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

  // SessionObserver:
  void OnActiveUserPrefServiceChanged(PrefService* prefs) override;

  // Resets nudge state and show nudge timer.
  void HandleNudgeShown();

  // Checks whether we should show the context menu 'new' badge.
  bool ShouldShowNewFeatureBadge();

  // Increment the 'new' feature badge shown count.
  void MarkNewFeatureBadgeShown();

  // Increment the screenshot notification shown count.
  void MarkScreenshotNotificationShown();

  // ClipboardHistoryControllerImpl:
  void OnClipboardHistoryMenuShown(
      crosapi::mojom::ClipboardHistoryControllerShowSource show_source)
      override;
  void OnClipboardHistoryPasted() override;

  // Shows the nudge widget.
  void ShowNudge(ClipboardNudgeType nudge_type);

  // Ensure the destruction of a clipboard nudge that is animating.
  void ForceCloseAnimatingNudge();

  // Test methods for overriding and resetting the clock used by GetTime.
  void OverrideClockForTesting(base::Clock* test_clock);
  void ClearClockOverrideForTesting();

  const ClipboardState& GetClipboardStateForTesting();
  ClipboardNudge* GetClipboardNudgeForTesting() { return nudge_.get(); }

  // Test method for triggering the nudge timer to hide.
  void FireHideNudgeTimerForTesting();

 private:
  // Gets the number of times the nudge has been shown.
  int GetShownCount(PrefService* prefs);
  // Gets the last time the nudge was shown.
  base::Time GetLastShownTime(PrefService* prefs);
  // Gets the number of times the context menu 'new' badge has been shown.
  int GetNewFeatureBadgeShownCount(PrefService* prefs);
  // Checks whether another nudge can be shown.
  bool ShouldShowNudge(PrefService* prefs);
  // Gets the current time. Can be overridden for testing.
  base::Time GetTime();

  // Hides the nudge widget.
  void HideNudge();

  // Begins the animation for fading in or fading out the clipboard nudge.
  void StartFadeAnimation(bool show);

  // Time the nudge was last shown.
  TimeMetricHelper last_shown_time_;

  // Time the zero state nudge was last shown.
  TimeMetricHelper zero_state_last_shown_time_;

  // Time the new feature badge was last shown.
  TimeMetricHelper new_feature_last_shown_time_;

  // Time the screenshot notification nudge was last shown.
  TimeMetricHelper screenshot_notification_last_shown_time_;

  // Owned by ClipboardHistoryController.
  const ClipboardHistory* clipboard_history_;

  // Owned by ash/Shell.
  const ClipboardHistoryControllerImpl* const clipboard_history_controller_;

  // Current clipboard state.
  ClipboardState clipboard_state_ = ClipboardState::kInit;
  // The timestamp of the most recent paste.
  base::Time last_paste_timestamp_;

  // Contextual nudge which shows a view to inform the user on multipaste usage.
  std::unique_ptr<ClipboardNudge> nudge_;

  // Timer to hide the clipboard nudge.
  base::OneShotTimer hide_nudge_timer_;

  std::unique_ptr<ui::ImplicitAnimationObserver> hide_nudge_animation_observer_;

  base::WeakPtrFactory<ClipboardNudgeController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONTROLLER_H_
