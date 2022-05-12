// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_edit_menu.h"
#include "chrome/browser/ash/arc/input_overlay/ui/error_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Widget;
}  // namespace views

namespace arc {
class ArcInputOverlayManagerTest;
namespace input_overlay {
class TouchInjector;
class InputMappingView;
class InputMenuView;
class ActionEditMenu;
class EditModeExitView;
class ErrorView;
class EducationalView;

// DisplayOverlayController manages the input mapping view, view and edit mode,
// menu, and educational dialog. It also handles the visibility of the
// |ActionEditMenu| and |ErrorView| by listening to the |LocatedEvent|.
class DisplayOverlayController : public ui::EventHandler {
 public:
  DisplayOverlayController(TouchInjector* touch_injector, bool first_launch);
  DisplayOverlayController(const DisplayOverlayController&) = delete;
  DisplayOverlayController& operator=(const DisplayOverlayController&) = delete;
  ~DisplayOverlayController() override;

  void OnWindowBoundsChanged();
  void SetDisplayMode(DisplayMode mode);
  // Get the bounds of |overlay_menu_entry_| in contents view.
  absl::optional<gfx::Rect> GetOverlayMenuEntryBounds();

  void AddActionEditMenu(ActionView* anchor, ActionType action_type);
  void RemoveActionEditMenu();

  void AddEditErrorMsg(ActionView* action_view, base::StringPiece error_msg);
  void RemoveEditErrorMsg();

  void OnBindingChange(Action* action,
                       std::unique_ptr<InputElement> input_element);

  // Save the changes when users press the save button after editing.
  void OnCustomizeSave();
  // Don't save any changes when users press the cancel button after editing.
  void OnCustomizeCancel();
  // Restore back to original default binding when users press the restore
  // button after editing.
  void OnCustomizeRestore();
  const std::string* GetPackageName() const;
  // Once the menu state is loaded from protobuf data, it should be applied on
  // the view. For example, |InputMappingView| may not be visible if it is
  // hidden or input overlay is disabled.
  void OnApplyMenuState();

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

 private:
  friend class ::arc::ArcInputOverlayManagerTest;
  friend class DisplayOverlayControllerTest;
  friend class EducationalView;
  friend class InputMenuView;
  friend class InputMappingView;

  // Display overlay is added for starting |display_mode|.
  void AddOverlay(DisplayMode display_mode);
  void RemoveOverlayIfAny();

  void AddMenuEntryView(views::Widget* overlay_widget);
  void RemoveMenuEntryView();
  void OnMenuEntryPressed();
  void RemoveInputMenuView();

  void AddInputMappingView(views::Widget* overlay_widget);
  void RemoveInputMappingView();

  void AddEditModeExitView(views::Widget* overlay_widget);
  void RemoveEditModeExitView();

  // Add |EducationalView|.
  void AddEducationalView();
  // Remove |EducationalView| and its references.
  void RemoveEducationalView();
  void OnEducationalViewDismissed();

  views::Widget* GetOverlayWidget();
  gfx::Point CalculateMenuEntryPosition();
  gfx::Point CalculateEditModeExitPosition();
  views::View* GetParentView();
  bool HasMenuView() const;
  void SetInputMappingVisible(bool visible);
  bool GetInputMappingViewVisible() const;

  void SetTouchInjectorEnable(bool enable);
  bool GetTouchInjectorEnable();

  // Close |ActionEditMenu| Or |ErrorView| if |LocatedEvent| happens outside of
  // their view bounds.
  void ProcessPressedEvent(const ui::LocatedEvent& event);

  // For test:
  gfx::Rect GetInputMappingViewBoundsForTesting();
  void DismissEducationalViewForTesting();

  TouchInjector* touch_injector() { return touch_injector_; }

  const raw_ptr<TouchInjector> touch_injector_;

  // References to UI elements owned by the overlay widget.
  raw_ptr<InputMappingView> input_mapping_view_ = nullptr;
  raw_ptr<InputMenuView> input_menu_view_ = nullptr;
  raw_ptr<views::ImageButton> menu_entry_ = nullptr;
  raw_ptr<ActionEditMenu> action_edit_menu_ = nullptr;
  raw_ptr<EditModeExitView> edit_mode_view_ = nullptr;
  raw_ptr<ErrorView> error_ = nullptr;
  raw_ptr<EducationalView> educational_view_ = nullptr;

  DisplayMode display_mode_ = DisplayMode::kNone;
};

}  // namespace input_overlay
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_
