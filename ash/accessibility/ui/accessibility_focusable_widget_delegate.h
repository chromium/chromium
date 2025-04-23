// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_UI_ACCESSIBILITY_FOCUSABLE_WIDGET_DELEGATE_H_
#define ASH_ACCESSIBILITY_UI_ACCESSIBILITY_FOCUSABLE_WIDGET_DELEGATE_H_

#include <type_traits>

#include "ash/focus/focus_cycler.h"
#include "ash/shell.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {

// A mix-in template for a `views::WidgetDelegate` that allows a non activatable
// widget to follow the accessibility keyboard natviation.
template <typename T>
class AccessibilityFocusable : public T {
 public:
  explicit AccessibilityFocusable(bool register_with_focus_cycler = false)
      : register_with_focus_cycler_(register_with_focus_cycler) {
    static_assert(std::is_base_of<views::WidgetDelegate, T>::value,
                  "The typename must be derived from WidgetDelegate");
  }

  // views::WidgetDelegate:

  bool CanActivate() const override {
    return Shell::Get()->focus_cycler()->widget_activating() == T::GetWidget();
  }

  void OnWidgetInitialized() override {
    if (register_with_focus_cycler_) {
      Shell::Get()->focus_cycler()->AddWidget(T::GetWidget());
    }
  }

  void WindowClosing() override {
    if (register_with_focus_cycler_) {
      Shell::Get()->focus_cycler()->RemoveWidget(T::GetWidget());
    }
  }

 private:
  const bool register_with_focus_cycler_;
};

// A concreate class that can be used as a `views::WidgetDelegate` impl.
class AccessibilityFocusableWidgetDelegate
    : public AccessibilityFocusable<views::WidgetDelegate> {
 public:
  explicit AccessibilityFocusableWidgetDelegate(
      bool register_with_focus_cycler = false);
  AccessibilityFocusableWidgetDelegate(
      const AccessibilityFocusableWidgetDelegate&) = delete;
  AccessibilityFocusableWidgetDelegate& operator=(
      const AccessibilityFocusableWidgetDelegate&) = delete;
  ~AccessibilityFocusableWidgetDelegate() override = default;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_UI_ACCESSIBILITY_FOCUSABLE_WIDGET_DELEGATE_H_
