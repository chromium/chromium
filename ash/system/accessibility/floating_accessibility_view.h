// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_FLOATING_ACCESSIBILITY_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_FLOATING_ACCESSIBILITY_VIEW_H_

#include <vector>

#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/shell_observer.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ui/views/controls/button/button.h"

namespace ash {

class FloatingMenuButton;
class TrayBackgroundView;

class FloatingAccessibilityBubbleView : public TrayBubbleView {
 public:
  explicit FloatingAccessibilityBubbleView(
      const TrayBubbleView::InitParams& init_params);
  FloatingAccessibilityBubbleView(const FloatingAccessibilityBubbleView&) =
      delete;
  FloatingAccessibilityBubbleView& operator=(
      const FloatingAccessibilityBubbleView&) = delete;
  ~FloatingAccessibilityBubbleView() override;

  // TrayBubbleView:
  bool IsAnchoredToStatusArea() const override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  // views::View:
  const char* GetClassName() const override;
};

// This floating view displays the currently enabled accessibility options,
// along with buttons to configure them.
// ---- Layout:
// ----  ?[Dictation] ?[SelectToSpeak] ?[VirtualKeyboard]
// ----  | [Open settings list]
// ----  | [Change menu location]
class FloatingAccessibilityView : public views::View,
                                  public views::ButtonListener,
                                  public views::ViewObserver {
 public:
  // Used for testing. Starts 1 because views IDs should not be 0.
  enum ButtonId {
    kPosition = 1,
    kSettingsList = 2,
    kDictation = 3,
    kSelectToSpeak = 4,
    kVirtualKeyboard = 5,
  };
  class Delegate {
   public:
    // When the user click on the settings list button.
    virtual void OnDetailedMenuEnabled(bool enabled) {}
    // When the layout of the view changes and we may need to reposition
    // ourselves.
    virtual void OnLayoutChanged() {}
    virtual ~Delegate() = default;
  };

  FloatingAccessibilityView(Delegate* delegate);
  FloatingAccessibilityView& operator=(const FloatingAccessibilityView&) =
      delete;
  ~FloatingAccessibilityView() override;
  FloatingAccessibilityView(const FloatingAccessibilityView&) = delete;

  // Initizlizes feature button views. Should be called after the view is
  // connected to a widget.
  void Initialize();

  void SetMenuPosition(FloatingMenuPosition position);
  void SetDetailedViewShown(bool shown);

  void FocusOnDetailedViewButton();

 private:
  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View:
  const char* GetClassName() const override;

  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override;
  // Feature buttons:
  TrayBackgroundView* dictation_button_ = nullptr;
  TrayBackgroundView* select_to_speak_button_ = nullptr;
  TrayBackgroundView* virtual_keyboard_button_ = nullptr;

  // Button to list all available features.
  FloatingMenuButton* a11y_tray_button_ = nullptr;
  // Button to move the view around corners.
  FloatingMenuButton* position_button_ = nullptr;

  Delegate* const delegate_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_FLOATING_ACCESSIBILITY_VIEW_H_
