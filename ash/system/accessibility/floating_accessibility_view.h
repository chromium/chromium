// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_FLOATING_ACCESSIBILITY_VIEW_H_
#define ASH_SYSTEM_ACCESSIBILITY_FLOATING_ACCESSIBILITY_VIEW_H_

#include "ash/public/cpp/accessibility_controller_enums.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/shell_observer.h"
#include "ash/system/tray/system_tray_observer.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/metadata/view_factory.h"

namespace ash {

class FloatingMenuButton;
class TrayBackgroundView;

class FloatingAccessibilityBubbleView : public TrayBubbleView {
  METADATA_HEADER(FloatingAccessibilityBubbleView, TrayBubbleView)

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

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   FloatingAccessibilityBubbleView,
                   TrayBubbleView)
END_VIEW_BUILDER

// This floating view displays the currently enabled accessibility options,
// along with buttons to configure them.
// ---- Layout:
// ----  ?[Dictation] ?[SelectToSpeak] ?[VirtualKeyboard]
// ----  | [Open settings list]
// ----  | [Change menu location]
class FloatingAccessibilityView : public views::BoxLayoutView,
                                  public views::ViewObserver,
                                  public KeyboardControllerObserver,
                                  public SystemTrayObserver {
  METADATA_HEADER(FloatingAccessibilityView, views::BoxLayoutView)

 public:
  // Used for testing. Starts 1 because views IDs should not be 0.
  enum ButtonId {
    kPosition = 1,
    kSettingsList = 2,
    kDictation = 3,
    kSelectToSpeak = 4,
    kVirtualKeyboard = 5,
    kIme = 6,
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

  explicit FloatingAccessibilityView(Delegate* delegate);
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
  friend class FloatingAccessibilityControllerTest;

  void OnA11yTrayButtonPressed();
  void OnPositionButtonPressed();

  // views::ViewObserver:
  void OnViewVisibilityChanged(views::View* observed_view,
                               views::View* starting_view) override;

  // KeyboardControllerObserver:
  void OnKeyboardVisibilityChanged(bool visible) override;

  // SystemTrayObserver:
  void OnFocusLeavingSystemTray(bool reverse) override;
  void OnImeMenuTrayBubbleShown() override;

  TrayBackgroundView* dictation_button() {
    return dictation_button_observation_.GetSource();
  }

  TrayBackgroundView* select_to_speak_button() {
    return select_to_speak_button_observation_.GetSource();
  }

  TrayBackgroundView* virtual_keyboard_button() {
    return virtual_keyboard_button_observation_.GetSource();
  }

  ImeMenuTray* ime_button() { return ime_button_observation_.GetSource(); }

  // Feature buttons:
  base::ScopedObservation<TrayBackgroundView, ViewObserver>
      dictation_button_observation_{this};
  base::ScopedObservation<TrayBackgroundView, ViewObserver>
      select_to_speak_button_observation_{this};
  base::ScopedObservation<TrayBackgroundView, ViewObserver>
      virtual_keyboard_button_observation_{this};

  // Button to list all available features.
  raw_ptr<FloatingMenuButton> a11y_tray_button_ = nullptr;
  // Button to move the view around corners.
  raw_ptr<FloatingMenuButton> position_button_ = nullptr;
  // Button to list all available keyboard languages.
  base::ScopedObservation<ImeMenuTray, ViewObserver> ime_button_observation_{
      this};

  const raw_ptr<Delegate> delegate_;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   FloatingAccessibilityView,
                   views::BoxLayoutView)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::FloatingAccessibilityBubbleView)
DEFINE_VIEW_BUILDER(/* no export */, ash::FloatingAccessibilityView)

#endif  // ASH_SYSTEM_ACCESSIBILITY_FLOATING_ACCESSIBILITY_VIEW_H_
