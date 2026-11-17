// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_HOTKEY_HOLD_WATCHER_H_
#define CHROME_BROWSER_DICTATION_HOTKEY_HOLD_WATCHER_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/dictation/stream_provider.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/event_observer.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/views/widget/widget_observer.h"

class BrowserWindowInterface;

namespace views {
class EventMonitor;
class Widget;
}  // namespace views

namespace dictation {

// Temporary, observe-only watcher created when Dictation is started via the
// hotkey. For the first `hold_threshold` after press, releasing the key keeps
// the stream active in toggle mode (`nullopt` trigger). Once held for at least
// `hold_threshold`, the session switches to press-and-hold mode and releasing
// the primary key or any modifier key ends the stream with
// `kHotkeyHoldReleased`. Aborts (blur, tab switch, secondary key press) while
// in press-and-hold mode end the stream with `kHotkeyHoldAbort`.
class HotkeyHoldWatcher : public ui::EventObserver,
                          public views::WidgetObserver {
 public:
  // `end_trigger` is `nullopt` if the key was released or the watch was aborted
  // before `hold_threshold` (keeping Dictation active in toggle mode), or the
  // trigger to end the stream with (`kHotkeyHoldReleased` or
  // `kHotkeyHoldAbort`).
  using CompletionCallback =
      base::OnceCallback<void(std::optional<DictationStreamEndTrigger>)>;

  HotkeyHoldWatcher(BrowserWindowInterface& browser,
                    tabs::TabInterface& tab,
                    views::Widget& browser_widget,
                    const ui::Accelerator& hotkey,
                    CompletionCallback completion_callback,
                    base::TimeDelta hold_threshold);
  HotkeyHoldWatcher(const HotkeyHoldWatcher&) = delete;
  HotkeyHoldWatcher& operator=(const HotkeyHoldWatcher&) = delete;
  ~HotkeyHoldWatcher() override;

  // Returns true if `key_code` is the main key or one of the required
  // modifier keys of `hotkey`.
  static bool IsChordKey(const ui::Accelerator& hotkey,
                         ui::KeyboardCode key_code);

  bool is_press_and_hold_mode() const { return is_press_and_hold_mode_; }
  base::TimeDelta hold_threshold_for_testing() const {
    return hold_threshold_timer_.GetCurrentDelay();
  }

  // ui::EventObserver:
  void OnEvent(const ui::Event& event) override;

 private:
  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  void OnHoldThresholdReached();
  void OnBrowserDidBecomeInactive(BrowserWindowInterface* browser);
  void OnTabWillDeactivate(tabs::TabInterface* tab);
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);
  void AbortAndDestroyThis();
  void FinishAndDestroyThis(
      std::optional<DictationStreamEndTrigger> end_trigger);

  const ui::Accelerator hotkey_;
  CompletionCallback completion_callback_;
  bool is_press_and_hold_mode_ = false;
  base::OneShotTimer hold_threshold_timer_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::CallbackListSubscription browser_inactive_subscription_;
  base::CallbackListSubscription tab_deactivate_subscription_;
  base::CallbackListSubscription tab_detach_subscription_;

  std::unique_ptr<views::EventMonitor> event_monitor_;
};

}  // namespace dictation

#endif  // CHROME_BROWSER_DICTATION_HOTKEY_HOLD_WATCHER_H_
