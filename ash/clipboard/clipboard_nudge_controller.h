// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONTROLLER_H_
#define ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/clipboard/clipboard_history.h"
#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "ui/base/clipboard/clipboard_observer.h"

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

class ASH_EXPORT ClipboardNudgeController : public ClipboardHistory::Observer,
                                            public ui::ClipboardObserver {
 public:
  ClipboardNudgeController(ClipboardHistory* clipboard_history);
  ClipboardNudgeController(const ClipboardNudgeController&) = delete;
  ClipboardNudgeController& operator=(const ClipboardNudgeController&) = delete;
  ~ClipboardNudgeController() override;

  // Registers profile prefs.
  static void RegisterProfilePrefs(PrefRegistrySimple* registry);

  // ui::ClipboardHistory::Observer:
  void OnClipboardHistoryItemAdded(const ClipboardHistoryItem& item) override;

  // ui::ClipboardObserver:
  void OnClipboardDataRead() override;

  // Resets nudge state and show nudge timer.
  void HandleNudgeShown();

  // Test methods for overriding and resetting the clock used by GetTime.
  void OverrideClockForTesting(base::Clock* test_clock);
  void ClearClockOverrideForTesting();

  const ClipboardState& GetClipboardStateForTesting();
  ClipboardNudge* GetClipboardNudgeForTesting() { return nudge_.get(); }

 private:
  // Gets the number of times the nudge has been shown.
  int GetShownCount(PrefService* prefs);
  // Gets the last time the nudge was shown.
  base::Time GetLastShownTime(PrefService* prefs);
  // Checks whether another nudge can be shown.
  bool ShouldShowNudge(PrefService* prefs);
  // Gets the current time. Can be overridden for testing.
  base::Time GetTime();

  // Shows the nudge widget.
  void ShowNudge();

  // Hides the nudge widget.
  void HideNudge();

  // Owned by ClipboardHistoryController.
  const ClipboardHistory* clipboard_history_;

  // Current clipboard state.
  ClipboardState clipboard_state_ = ClipboardState::kInit;
  // The timestamp of the most recent paste.
  base::Time last_paste_timestamp_;
  // Clock that can be overridden for testing.
  base::Clock* g_clock_override = nullptr;

  // Contextual nudge which shows a view to inform the user on multipaste usage.
  std::unique_ptr<ClipboardNudge> nudge_;

  // Timer to hide the clipboard nudge.
  base::OneShotTimer hide_nudge_timer_;

  base::WeakPtrFactory<ClipboardNudgeController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_CLIPBOARD_CLIPBOARD_NUDGE_CONTROLLER_H_
