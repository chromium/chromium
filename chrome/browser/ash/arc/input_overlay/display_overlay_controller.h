// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_

#include <string>
#include <vector>

#include "ash/public/cpp/arc_game_controls_flag.h"
#include "ash/public/cpp/window_properties.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/property_change_reason.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace arc::input_overlay {

class Action;
class ActionEditMenu;
class ActionViewListItem;
class ButtonOptionsMenu;
class EditFinishView;
class EditingList;
class EducationalView;
class InputMappingView;
class InputMenuView;
class MenuEntryView;
class MessageView;
class NudgeView;
class TouchInjector;
class TouchInjectorObserver;

// DisplayOverlayController manages the input mapping view, view and edit mode,
// menu, and educational dialog. It also handles the visibility of the
// `ActionEditMenu` and `MessageView` by listening to the `LocatedEvent`.
class DisplayOverlayController : public ui::EventHandler,
                                 public aura::WindowObserver {
 public:
  DisplayOverlayController(TouchInjector* touch_injector, bool first_launch);
  DisplayOverlayController(const DisplayOverlayController&) = delete;
  DisplayOverlayController& operator=(const DisplayOverlayController&) = delete;
  ~DisplayOverlayController() override;

  void SetDisplayModeAlpha(DisplayMode mode);
  void SetDisplayMode(DisplayMode mode);

  // Get the bounds of `menu_entry_` in screen coordinates.
  absl::optional<gfx::Rect> GetOverlayMenuEntryBounds();

  void AddEditMessage(const base::StringPiece& message,
                      MessageType message_type);
  void RemoveEditMessage();

  void OnInputBindingChange(Action* action,
                            std::unique_ptr<InputElement> input_element);

  // Save changes to actions, without changing the display mode afterward.
  void SaveToProtoFile();
  // Save the changes when users press the save button after editing.
  void OnCustomizeSave();
  // Don't save any changes when users press the cancel button after editing.
  void OnCustomizeCancel();
  // Restore back to original default binding when users press the restore
  // button after editing.
  void OnCustomizeRestore();
  const std::string& GetPackageName() const;
  // Once the menu state is loaded from protobuf data, it should be applied on
  // the view. For example, `InputMappingView` may not be visible if it is
  // hidden or input overlay is disabled.
  void OnApplyMenuState();
  // Get window state type.
  InputOverlayWindowStateType GetWindowStateType() const;

  // For editor.
  void AddNewAction(ActionType action_type = ActionType::TAP);
  void RemoveAction(Action* action);
  // Creates a new action with guidance from the reference action, and deletes
  // the reference action.
  void ChangeActionType(Action* reference_action_, ActionType type);
  void ChangeActionName(Action* action, int index);
  void RemoveActionNewState(Action* action);

  // Returns the size of active actions which include the deleted default
  // actions.
  size_t GetActiveActionsSize();
  // Return true if action is not deleted.
  bool IsActiveAction(Action* action);

  // For menu entry hover state:
  void SetMenuEntryHoverState(bool curr_hover_state);

  // Add UIs to observer touch injector change.
  void AddTouchInjectorObserver(TouchInjectorObserver* observer);
  void RemoveTouchInjectorObserver(TouchInjectorObserver* observer);

  void AddButtonOptionsMenuWidget(Action* action);
  void RemoveButtonOptionsMenuWidget();
  void SetButtonOptionsMenuWidgetVisibility(bool is_visible);

  void AddNudgeWidget(views::View* anchor_view, const std::u16string& text);
  void RemoveNudgeWidget(views::Widget* widget);

  void AddDeleteEditShortcutWidget(ActionViewListItem* anchor_view);
  void RemoveDeleteEditShortcutWidget();

  // Show education nudge for editing tip. It only shows up for the first new
  // action after closing `ButtonOptionsMenu`.
  void MayShowEduNudgeForEditingTip();

  // Update widget bounds if the view content is changed or the app window
  // bounds are changed.
  void UpdateButtonOptionsMenuWidgetBounds(Action* action);
  void UpdateInputMappingWidgetBounds();
  void UpdateEditingListWidgetBounds();

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  const TouchInjector* touch_injector() const { return touch_injector_; }

 private:
  friend class ActionView;
  friend class ArcInputOverlayManagerTest;
  friend class ButtonOptionsMenu;
  friend class DisplayOverlayControllerTest;
  friend class DisplayOverlayControllerAlphaTest;
  friend class EditingList;
  friend class EditingListTest;
  friend class EditLabelTest;
  friend class EducationalView;
  friend class GameControlsTestBase;
  friend class InputMappingView;
  friend class InputMenuView;
  friend class MenuEntryView;
  friend class MenuEntryViewTest;
  friend class OverlayViewTestBase;

  // Display overlay is added for starting `display_mode`.
  void AddOverlay(DisplayMode display_mode);
  void RemoveOverlayIfAny();
  // If `on_overlay` is true, set event target on overlay layer. Otherwise, set
  // event target on the layer underneath the overlay layer.
  void SetEventTarget(views::Widget* overlay_widget, bool on_overlay);

  // Update display mode when initializing DisplayOverlayController or the
  // window property is changed on `ash::kArcGameControlsFlagsKey`.
  void UpdateDisplayMode();

  // On charge of Add/Remove nudge view.
  void AddNudgeView(views::Widget* overlay_widget);
  void RemoveNudgeView();
  void OnNudgeDismissed();
  gfx::Point CalculateNudgePosition(int nudge_width);

  bool IsNudgeEmpty();

  void AddMenuEntryView(views::Widget* overlay_widget);
  void RemoveMenuEntryView();
  void OnMenuEntryPressed();
  void OnMenuEntryPositionChanged(bool leave_focus,
                                  absl::optional<gfx::Point> location);
  void FocusOnMenuEntry();
  void ClearFocus();
  void RemoveInputMenuView();

  void AddInputMappingView(views::Widget* overlay_widget);
  void RemoveInputMappingView();

  void AddEditFinishView(views::Widget* overlay_widget);
  void RemoveEditFinishView();

  // Add `EducationalView`.
  void AddEducationalView();
  // Remove `EducationalView` and its references.
  void RemoveEducationalView();
  void OnEducationalViewDismissed();

  views::Widget* GetOverlayWidget();
  views::View* GetOverlayWidgetContentsView();
  bool HasMenuView() const;
  // Used for edit mode, in which the input mapping must be temporarily visible
  // regardless of user setting, until it is overridden when the user presses
  // save or cancel.
  void SetInputMappingVisibleTemporary();
  // Used for the mapping hint toggle, to save user settings regarding
  // mapping hint visibility.
  void SetInputMappingVisible(bool visible);
  bool GetInputMappingViewVisible() const;

  void SetTouchInjectorEnable(bool enable);
  bool GetTouchInjectorEnable();
  // Used for the magnetic function of the editing list.

  // Close `MessageView` if `LocatedEvent` happens outside
  // of their view bounds.
  void ProcessPressedEvent(const ui::LocatedEvent& event);

  // When the input is processed on overlay in edit mode, PlaceholderActivity
  // task window becomes the front task window. This ensures the target task
  // window is moved back to the front of task stack on ARC side for view mode.
  void EnsureTaskWindowToFrontForViewMode(views::Widget* overlay_widget);

  void UpdateForBoundsChanged();

  // For beta.
  void RemoveAllWidgets();

  void AddInputMappingWidget();
  void RemoveInputMappingWidget();
  InputMappingView* GetInputMapping();

  void AddEditingListWidget();
  void RemoveEditingListWidget();
  EditingList* GetEditingList();

  ButtonOptionsMenu* GetButtonOptionsMenu();

  // `widget` bounds is in screen coordinate. `bounds_in_root_window` is the
  // window bounds in root window. Convert `bounds_in_root_window` in screen
  // coordinates to set `widget` bounds.
  void UpdateWidgetBoundsInRootWindow(views::Widget* widget,
                                      const gfx::Rect& bounds_in_root_window);

  // `TouchInjector` only rewrite events in `kView` mode. When changing between
  // edit mode and view mode or the feature is disabled from menu or if the game
  // dashboard menu shows up, it needs to tell `TouchInjector` if it can rewrite
  // events.
  void UpdateEventRewriteCapability();

  // For test:
  gfx::Rect GetInputMappingViewBoundsForTesting();
  void DismissEducationalViewForTesting();
  InputMenuView* GetInputMenuView() { return input_menu_view_; }
  MenuEntryView* GetMenuEntryView() { return menu_entry_; }

  const raw_ptr<TouchInjector> touch_injector_;

  // References to UI elements owned by the overlay widget.
  raw_ptr<InputMappingView, DanglingUntriaged> input_mapping_view_ = nullptr;
  raw_ptr<InputMenuView, DanglingUntriaged> input_menu_view_ = nullptr;
  raw_ptr<MenuEntryView, DanglingUntriaged> menu_entry_ = nullptr;
  raw_ptr<EditFinishView, DanglingUntriaged> edit_finish_view_ = nullptr;
  raw_ptr<MessageView, DanglingUntriaged> message_ = nullptr;
  raw_ptr<EducationalView, DanglingUntriaged> educational_view_ = nullptr;
  raw_ptr<NudgeView, DanglingUntriaged> nudge_view_ = nullptr;

  DisplayMode display_mode_ = DisplayMode::kNone;

  // For beta.
  std::unique_ptr<views::Widget> input_mapping_widget_;
  std::unique_ptr<views::Widget> editing_list_widget_;
  std::unique_ptr<views::Widget> button_options_widget_;
  std::unique_ptr<views::Widget> delete_edit_shortcut_widget_;

  // Each widget can associate with one education nudge widget.
  base::flat_map<views::Widget*, std::unique_ptr<views::Widget>> nudge_widgets_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_
