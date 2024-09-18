// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_

#include <string>
#include <string_view>

#include "ash/public/cpp/arc_game_controls_flag.h"
#include "ash/public/cpp/window_properties.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_multi_source_observation.h"
#include "chrome/browser/ash/arc/input_overlay/actions/input_element.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_metrics.h"
#include "ui/aura/window_observer.h"
#include "ui/compositor/property_change_reason.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class View;
class Widget;
}  // namespace views

namespace arc::input_overlay {

class Action;
class ActionEditMenu;
class ActionViewListItem;
class ButtonOptionsMenu;
class DeleteEditShortcut;
class EditFinishView;
class EditingList;
class EducationalView;
class InputMappingView;
class InputMenuView;
class MenuEntryView;
class MessageView;
class NudgeView;
class RichNudge;
class TargetView;
class TouchInjector;
class TouchInjectorObserver;

// DisplayOverlayController manages the input mapping view, view and edit mode,
// menu, and educational dialog. It also handles the visibility of the
// `ActionEditMenu` and `MessageView` by listening to the `LocatedEvent`.
class DisplayOverlayController : public ui::EventHandler,
                                 public aura::WindowObserver,
                                 public views::WidgetObserver {
 public:
  DisplayOverlayController(TouchInjector* touch_injector, bool first_launch);
  DisplayOverlayController(const DisplayOverlayController&) = delete;
  DisplayOverlayController& operator=(const DisplayOverlayController&) = delete;
  ~DisplayOverlayController() override;

  void SetDisplayModeAlpha(DisplayMode mode);
  void SetDisplayMode(DisplayMode mode);

  // Get the bounds of `menu_entry_` in screen coordinates.
  std::optional<gfx::Rect> GetOverlayMenuEntryBounds();

  void AddEditMessage(std::string_view message, MessageType message_type);
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
  void AddNewAction(ActionType action_type, const gfx::Point& target_pos);
  void RemoveAction(Action* action);
  // Creates a new action with guidance from the reference action, and deletes
  // the reference action.
  void ChangeActionType(Action* reference_action_, ActionType type);
  void RemoveActionNewState(Action* action);

  // Returns the size of active actions which include the deleted default
  // actions.
  size_t GetActiveActionsSize() const;
  // Returns true if there is only one user added action.
  bool HasSingleUserAddedAction() const;
  // Return true if action is not deleted.
  bool IsActiveAction(Action* action) const;

  MappingSource GetMappingSource() const;

  // For menu entry hover state:
  void SetMenuEntryHoverState(bool curr_hover_state);

  // Add UIs to observer touch injector change.
  void AddTouchInjectorObserver(TouchInjectorObserver* observer);
  void RemoveTouchInjectorObserver(TouchInjectorObserver* observer);

  void AddButtonOptionsMenuWidget(Action* action);
  void RemoveButtonOptionsMenuWidget();
  void SetButtonOptionsMenuWidgetVisibility(bool is_visible);

  void AddDeleteEditShortcutWidget(ActionViewListItem* anchor_view);
  void RemoveDeleteEditShortcutWidget();

  void EnterButtonPlaceMode(ActionType action_type);
  // Exits button placement mode after adding a new action if `is_action_added`
  // is true or giving up by pressing key `esc` if `is_action_added` is false.
  void ExitButtonPlaceMode(bool is_action_added);
  void UpdateButtonPlacementNudgeAnchorRect();

  void AddActionHighlightWidget(Action* action);
  void RemoveActionHighlightWidget();
  void HideActionHighlightWidget();
  // Hides the action highlight if the action highlight is anchored to
  // `action`'s view.
  void HideActionHighlightWidgetForAction(Action* action);

  // `widget` bounds is in screen coordinate. `bounds_in_root_window` is the
  // window bounds in associated game window's root window. Convert
  // `bounds_in_root_window` in screen coordinates to set `widget` bounds.
  void UpdateWidgetBoundsInRootWindow(views::Widget* widget,
                                      const gfx::Rect& bounds_in_root_window);

  ActionViewListItem* GetEditingListItemForAction(Action* action);

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnKeyEvent(ui::KeyEvent* event) override;

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  // views::WidgetObserver:
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetDestroying(views::Widget* widget) override;

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
  friend class RichNudgeTest;

  class FocusCycler;

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
                                  std::optional<gfx::Point> location);
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
  // Sets input mapping visibility according to `visible` and stores the setting
  // if `store_visible_state` is true.
  void SetInputMappingVisible(bool visible, bool store_visible_state = false);
  bool GetInputMappingViewVisible() const;

  void SetTouchInjectorEnable(bool enable);
  bool GetTouchInjectorEnable();

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

  // Stacks input mapping at the bottom and under the game dashboard UIs.
  void StackInputMappingAtBottomForViewMode();

  void AddEditingListWidget();
  void RemoveEditingListWidget();
  void SetEditingListVisibility(bool visible);
  EditingList* GetEditingList();

  ButtonOptionsMenu* GetButtonOptionsMenu();

  // Focus cycler operations.
  void AddFocusCycler();
  void RemoveFocusCycler();

  // Shows or removes target view when in or out button place mode.
  void AddTargetWidget(ActionType action_type);
  void RemoveTargetWidget();
  TargetView* GetTargetView() const;

  void AddRichNudge();
  void RemoveRichNudge();
  RichNudge* GetRichNudge() const;

  DeleteEditShortcut* GetDeleteEditShortcut() const;

  // Update widget bounds if the view content is changed or the app window
  // bounds are changed.
  void UpdateButtonOptionsMenuWidgetBounds();
  void UpdateInputMappingWidgetBounds();
  void UpdateEditingListWidgetBounds();
  void UpdateTargetWidgetBounds();

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

  base::ScopedMultiSourceObservation<views::Widget, views::WidgetObserver>
      widget_observations_{this};

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
  std::unique_ptr<views::Widget> target_widget_;
  std::unique_ptr<views::Widget> action_highlight_widget_;
  views::UniqueWidgetPtr delete_edit_shortcut_widget_;
  views::UniqueWidgetPtr rich_nudge_widget_;

  std::unique_ptr<FocusCycler> focus_cycler_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_DISPLAY_OVERLAY_CONTROLLER_H_
