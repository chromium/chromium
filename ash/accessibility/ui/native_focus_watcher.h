// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_UI_NATIVE_FOCUS_WATCHER_H_
#define ASH_ACCESSIBILITY_UI_NATIVE_FOCUS_WATCHER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/native_view_focus_manager.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {

// Observes changes to the bounds of the natively focused element.
class NativeFocusObserver : public base::CheckedObserver {
 public:
  virtual void OnNativeFocusChanged(const gfx::Rect& bounds_in_screen) = 0;
  virtual void OnNativeFocusCleared() = 0;

 protected:
  ~NativeFocusObserver() override = default;
};

// Watches for native focus changes across all windows and widgets. Calls
// registered `NativeFocusObserver`s on focus changes.
class NativeFocusWatcher : public views::NativeViewFocusChangeListener,
                           public views::FocusChangeListener,
                           public views::WidgetObserver {
 public:
  NativeFocusWatcher();
  NativeFocusWatcher(const NativeFocusWatcher&) = delete;
  NativeFocusWatcher& operator=(const NativeFocusWatcher&) = delete;
  ~NativeFocusWatcher() override;

  // Starts or stops watching for native focus changes.
  void SetEnabled(bool enabled);

  void AddObserver(NativeFocusObserver* observer);
  void RemoveObserver(NativeFocusObserver* observer);

 private:
  // Sets the focused |widget|.
  void SetWidget(views::Widget* widget);

  // Updates the focus rect and calls the observers.
  void UpdateFocusedView();

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // views::NativeViewFocusChangeListener:
  void OnNativeFocusChanged(gfx::NativeView focused_now) override;

  // views::FocusChangeListener:
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  bool enabled_ = false;

  base::ObserverList<NativeFocusObserver> observers_;

  raw_ptr<views::Widget> widget_ = nullptr;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_UI_NATIVE_FOCUS_WATCHER_H_
