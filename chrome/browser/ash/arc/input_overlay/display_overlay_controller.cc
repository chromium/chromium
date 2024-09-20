// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"

#include <algorithm>
#include <memory>

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/game_dashboard/game_dashboard_controller.h"
#include "ash/game_dashboard/game_dashboard_utils.h"
#include "ash/public/cpp/arc_game_controls_flag.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/style/style_util.h"
#include "ash/wm/window_state.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/arc_input_overlay_metrics.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_highlight.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view_list_item.h"
#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"
#include "chrome/browser/ash/arc/input_overlay/ui/delete_edit_shortcut.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_finish_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"
#include "chrome/browser/ash/arc/input_overlay/ui/educational_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_menu_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/menu_entry_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/message_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/nudge_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/rich_nudge.h"
#include "chrome/browser/ash/arc/input_overlay/ui/target_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/ui_utils.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace arc::input_overlay {

namespace {

using ash::game_dashboard_utils::GetNextWidgetToFocus;
using ash::game_dashboard_utils::UpdateAccessibilityTree;

// UI specs.
constexpr int kMenuEntrySideMargin = 24;
constexpr int kNudgeVerticalAlign = 8;

constexpr char kButtonOptionsMenu[] = "GameControlsButtonOptionsMenu";
constexpr char kEditingList[] = "GameControlsEditingList";
constexpr char kInputMapping[] = "GameControlsInputMapping";
constexpr char kActionHighlight[] = "ActionHighlight";

std::unique_ptr<views::Widget> CreateTransientWidget(
    aura::Window* parent_window,
    const std::string& widget_name,
    bool accept_events) {
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.parent = parent_window;
  params.name = widget_name;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.accept_events = accept_events;

  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));

  auto* widget_window = widget->GetNativeWindow();
  DCHECK_EQ(parent_window, wm::GetTransientParent(widget_window));
  wm::TransientWindowManager::GetOrCreate(widget_window)
      ->set_parent_controls_visibility(false);
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  return widget;
}

}  // namespace

// -----------------------------------------------------------------------------
// DisplayOverlayController::FocusCycler:

class DisplayOverlayController::FocusCycler {
 public:
  FocusCycler() {}
  ~FocusCycler() = default;

  // Adds `widget` to `widget_list_` if `widget` is visible and not in
  // `widget_list_`. Otherwise, removes `widget` from `widget_list_`.
  void RefreshWidget(views::Widget* widget) {
    if (widget->IsVisible()) {
      AddWidget(widget);
    } else {
      RemoveWidget(widget);
    }
  }

  void MaybeChangeFocusWidget(ui::KeyEvent& event) {
    // Only tab pressed is checked because the focus change is triggered by the
    // tab pressed event. No need to change widget focus again on tab key
    // released event.
    if (event.type() == ui::EventType::kKeyReleased ||
        !views::FocusManager::IsTabTraversalKeyEvent(event)) {
      return;
    }

    const bool reverse = event.IsShiftDown();
    auto* target_widget = views::Widget::GetWidgetForNativeWindow(
        static_cast<aura::Window*>(event.target()));
    auto* focus_manager = target_widget->GetFocusManager();

    // Once there is next focusable view (dont_loop==true), it means the current
    // focus is not the first or the last focusable view, so it doesn't need to
    // change focus to the next widget.
    if (focus_manager->GetNextFocusableView(
            /*starting_view=*/focus_manager->GetFocusedView(),
            /*starting_widget=*/target_widget, /*reverse=*/reverse,
            /*dont_loop=*/true)) {
      return;
    }

    // Change focus to the next widget.
    if (auto* next_widget =
            GetNextWidgetToFocus(widget_list_, target_widget, reverse)) {
      next_widget->GetFocusManager()->AdvanceFocus(reverse);
      // Change the event target.
      ui::Event::DispatcherApi(&event).set_target(
          next_widget->GetNativeWindow());
    }
  }

 private:
  void AddWidget(views::Widget* widget) {
    if (auto it = std::find(widget_list_.begin(), widget_list_.end(), widget);
        it == widget_list_.end()) {
      widget_list_.emplace_back(widget);
      UpdateAccessibilityTree(widget_list_);
    }
  }

  void RemoveWidget(views::Widget* widget) {
    if (auto it = std::find(widget_list_.begin(), widget_list_.end(), widget);
        it != widget_list_.end()) {
      widget_list_.erase(it);
      UpdateAccessibilityTree(widget_list_);
    }
  }

  // Only contains visible and unique widgets.
  std::vector<views::Widget*> widget_list_;
};

// -----------------------------------------------------------------------------
// DisplayOverlayController:

DisplayOverlayController::DisplayOverlayController(
    TouchInjector* touch_injector,
    bool first_launch)
    : touch_injector_(touch_injector) {
  touch_injector_->set_display_overlay_controller(this);

  if (IsBeta()) {
    auto* window = touch_injector_->window();
    window->AddObserver(this);

    UpdateDisplayMode();
  } else {
    // There is no instance for unittest.
    if (!ash::Shell::HasInstance()) {
      return;
    }

    AddOverlay(first_launch ? DisplayMode::kEducation : DisplayMode::kView);
  }
  ash::Shell::Get()->AddPreTargetHandler(this);
}

DisplayOverlayController::~DisplayOverlayController() {
  touch_injector_->set_display_overlay_controller(nullptr);

  if (IsBeta()) {
    widget_observations_.RemoveAllObservations();
    touch_injector_->window()->RemoveObserver(this);
    RemoveAllWidgets();
  } else {
    // There is no instance for unittest.
    if (!ash::Shell::HasInstance()) {
      return;
    }
    RemoveOverlayIfAny();
  }
  ash::Shell::Get()->RemovePreTargetHandler(this);
}

// For test:
gfx::Rect DisplayOverlayController::GetInputMappingViewBoundsForTesting() {
  return input_mapping_view_ ? input_mapping_view_->bounds() : gfx::Rect();
}

void DisplayOverlayController::AddOverlay(DisplayMode display_mode) {
  RemoveOverlayIfAny();
  touch_injector_->window()->AddObserver(this);

  auto* shell_surface_base =
      exo::GetShellSurfaceBaseForWindow(touch_injector_->window());
  if (!shell_surface_base) {
    return;
  }

  auto view = std::make_unique<views::View>();
  exo::ShellSurfaceBase::OverlayParams params(std::move(view));
  params.translucent = true;
  params.overlaps_frame = false;
  params.focusable = true;
  shell_surface_base->AddOverlay(std::move(params));

  SetDisplayModeAlpha(display_mode);
}

void DisplayOverlayController::RemoveOverlayIfAny() {
  if (display_mode_ == DisplayMode::kEdit) {
    OnCustomizeCancel();
  }
  if (auto* shell_surface_base =
          exo::GetShellSurfaceBaseForWindow(touch_injector_->window());
      shell_surface_base && shell_surface_base->HasOverlay()) {
    // Call `RemoveInputMenuView` explicitly to make sure UMA stats is updated.
    RemoveInputMenuView();

    shell_surface_base->RemoveOverlay();
  }

  touch_injector_->window()->RemoveObserver(this);
}

void DisplayOverlayController::SetEventTarget(views::Widget* overlay_widget,
                                              bool on_overlay) {
  auto* overlay_window = overlay_widget->GetNativeWindow();
  if (on_overlay) {
    overlay_window->SetEventTargetingPolicy(
        aura::EventTargetingPolicy::kTargetAndDescendants);
  } else {
    overlay_window->SetEventTargetingPolicy(aura::EventTargetingPolicy::kNone);
    EnsureTaskWindowToFrontForViewMode(overlay_widget);
  }
}

void DisplayOverlayController::UpdateDisplayMode() {
  ash::ArcGameControlsFlag flags =
      touch_injector_->window()->GetProperty(ash::kArcGameControlsFlagsKey);
  SetDisplayMode(IsFlagSet(flags, ash::ArcGameControlsFlag::kEnabled)
                     ? (IsFlagSet(flags, ash::ArcGameControlsFlag::kEdit)
                            ? DisplayMode::kEdit
                            : DisplayMode::kView)
                     : DisplayMode::kNone);
}

void DisplayOverlayController::AddNudgeView(views::Widget* overlay_widget) {
  DCHECK(overlay_widget);
  auto* parent = overlay_widget->GetContentsView();
  DCHECK(parent);
  if (!nudge_view_) {
    nudge_view_ = NudgeView::Show(parent, menu_entry_);
  }
}

void DisplayOverlayController::RemoveNudgeView() {
  if (!nudge_view_) {
    return;
  }

  nudge_view_->parent()->RemoveChildViewT(nudge_view_);
  nudge_view_ = nullptr;
}

void DisplayOverlayController::OnNudgeDismissed() {
  RemoveNudgeView();
  DCHECK(touch_injector_);
  touch_injector_->set_show_nudge(false);
}

void DisplayOverlayController::ChangeActionType(Action* reference_action,
                                                ActionType type) {
  touch_injector_->ChangeActionType(reference_action, type);
}

gfx::Point DisplayOverlayController::CalculateNudgePosition(int nudge_width) {
  gfx::Point nudge_position = menu_entry_->origin();
  int x = nudge_position.x() - nudge_width - kMenuEntrySideMargin;
  int y = nudge_position.y() + kNudgeVerticalAlign;
  // If the nudge view shows at the outside of the window, move the nudge view
  // down below the menu button and move it to left to make sure it shows inside
  // of the window.
  if (x < 0) {
    x = std::max(0, x + menu_entry_->width() + kMenuEntrySideMargin);
    y += menu_entry_->height();
  }

  return gfx::Point(x, y);
}

bool DisplayOverlayController::IsNudgeEmpty() {
  return !nudge_view_;
}

void DisplayOverlayController::AddMenuEntryView(views::Widget* overlay_widget) {
  if (menu_entry_) {
    menu_entry_->SetVisible(true);
    return;
  }
  DCHECK(overlay_widget);
  // Create and position entry point for `InputMenuView`.
  menu_entry_ = MenuEntryView::Show(
      base::BindRepeating(&DisplayOverlayController::OnMenuEntryPressed,
                          base::Unretained(this)),
      base::BindRepeating(&DisplayOverlayController::OnMenuEntryPositionChanged,
                          base::Unretained(this)),
      this);
}

void DisplayOverlayController::RemoveMenuEntryView() {
  if (!menu_entry_) {
    return;
  }
  menu_entry_->parent()->RemoveChildViewT(menu_entry_);
  menu_entry_ = nullptr;
}

void DisplayOverlayController::OnMenuEntryPressed() {
  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  auto* parent_view = overlay_widget->GetContentsView();
  DCHECK(parent_view);

  SetDisplayModeAlpha(DisplayMode::kMenu);

  input_menu_view_ = parent_view->AddChildView(
      InputMenuView::BuildMenuView(this, menu_entry_, parent_view->size()));
  // Hide the menu entry when the menu is displayed.
  menu_entry_->SetVisible(false);
}

void DisplayOverlayController::OnMenuEntryPositionChanged(
    bool leave_focus,
    std::optional<gfx::Point> location) {
  if (leave_focus) {
    SetDisplayModeAlpha(DisplayMode::kView);
  }

  if (location) {
    touch_injector_->SaveMenuEntryLocation(*location);
  }
}

void DisplayOverlayController::FocusOnMenuEntry() {
  if (!menu_entry_) {
    return;
  }
  menu_entry_->RequestFocus();
}

void DisplayOverlayController::ClearFocus() {
  auto* widget =
      views::Widget::GetWidgetForNativeWindow(touch_injector_->window());
  if (!widget) {
    return;
  }
  auto* focus_manager = widget->GetFocusManager();
  if (focus_manager) {
    focus_manager->ClearFocus();
  }
}

void DisplayOverlayController::RemoveInputMenuView() {
  if (!input_menu_view_) {
    return;
  }
  input_menu_view_->parent()->RemoveChildViewT(input_menu_view_);
  input_menu_view_ = nullptr;
  touch_injector_->OnInputMenuViewRemoved();
}

void DisplayOverlayController::AddInputMappingView(
    views::Widget* overlay_widget) {
  if (!input_mapping_view_) {
    DCHECK(overlay_widget);
    auto input_mapping_view = std::make_unique<InputMappingView>(this);
    input_mapping_view->SetPosition(gfx::Point());
    input_mapping_view_ = overlay_widget->GetContentsView()->AddChildView(
        std::move(input_mapping_view));
  }
  // Set input mapping view visibility according to the saved status.
  DCHECK(touch_injector_);
  SetInputMappingVisible(/*visible=*/touch_injector_->input_mapping_visible());
}

void DisplayOverlayController::RemoveInputMappingView() {
  if (!input_mapping_view_) {
    return;
  }
  input_mapping_view_->parent()->RemoveChildViewT(input_mapping_view_);
  input_mapping_view_ = nullptr;
}

void DisplayOverlayController::AddEditFinishView(
    views::Widget* overlay_widget) {
  DCHECK(overlay_widget);
  auto* parent_view = overlay_widget->GetContentsView();
  DCHECK(parent_view);

  edit_finish_view_ = EditFinishView::BuildView(this, parent_view);
}

void DisplayOverlayController::RemoveEditFinishView() {
  if (!edit_finish_view_) {
    return;
  }
  edit_finish_view_->parent()->RemoveChildViewT(edit_finish_view_);
  edit_finish_view_ = nullptr;
}

void DisplayOverlayController::AddEducationalView() {
  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  auto* parent_view = overlay_widget->GetContentsView();
  DCHECK(parent_view);
  if (educational_view_) {
    return;
  }

  educational_view_ =
      EducationalView::Show(this, GetOverlayWidgetContentsView());
}

void DisplayOverlayController::RemoveEducationalView() {
  if (!educational_view_) {
    return;
  }
  educational_view_->parent()->RemoveChildViewT(educational_view_);
  educational_view_ = nullptr;
}

void DisplayOverlayController::OnEducationalViewDismissed() {
  SetDisplayModeAlpha(DisplayMode::kView);
  DCHECK(touch_injector_);
  touch_injector_->set_first_launch(false);
}

views::Widget* DisplayOverlayController::GetOverlayWidget() {
  if (auto* shell_surface_base =
          exo::GetShellSurfaceBaseForWindow(touch_injector_->window())) {
    return static_cast<views::Widget*>(
        shell_surface_base->GetFocusTraversable());
  }

  // Shell surface is null for test.
  return nullptr;
}

views::View* DisplayOverlayController::GetOverlayWidgetContentsView() {
  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  return overlay_widget->GetContentsView();
}

void DisplayOverlayController::SetDisplayModeAlpha(DisplayMode mode) {
  DCHECK(!IsBeta());

  if (display_mode_ == mode) {
    return;
  }

  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  if (!overlay_widget) {
    return;
  }

  switch (mode) {
    case DisplayMode::kNone:
      RemoveEditMessage();
      RemoveMenuEntryView();
      RemoveInputMappingView();
      RemoveEducationalView();
      RemoveEditFinishView();
      RemoveNudgeView();
      break;
    case DisplayMode::kEducation:
      // Force recreating educational view as it is responsive to width changes.
      RemoveEducationalView();
      AddEducationalView();
      SetEventTarget(overlay_widget, /*on_overlay=*/true);
      break;
    case DisplayMode::kView:
      ClearFocus();
      RemoveEditMessage();
      RemoveInputMenuView();
      RemoveEditFinishView();
      RemoveEducationalView();
      RemoveNudgeView();
      AddInputMappingView(overlay_widget);
      AddMenuEntryView(overlay_widget);
      if (touch_injector_->show_nudge()) {
        AddNudgeView(overlay_widget);
      }
      SetEventTarget(overlay_widget, /*on_overlay=*/false);
      break;
    case DisplayMode::kEdit:
      // When using Tab to traverse views and enter into the edit mode, it needs
      // to reset the focus before removing the menu.
      ResetFocusTo(overlay_widget->GetContentsView());
      RemoveInputMenuView();
      RemoveMenuEntryView();
      RemoveEducationalView();
      RemoveNudgeView();
      AddEditFinishView(overlay_widget);
      SetEventTarget(overlay_widget, /*on_overlay=*/true);
      break;
    case DisplayMode::kPreMenu:
      RemoveNudgeView();
      SetEventTarget(overlay_widget, /*on_overlay=*/true);
      FocusOnMenuEntry();
      break;
    case DisplayMode::kMenu:
      SetEventTarget(overlay_widget, /*on_overlay=*/true);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      break;
  }

  if (input_mapping_view_) {
    input_mapping_view_->SetDisplayMode(mode);
  }

  DCHECK(touch_injector_);
  if (touch_injector_) {
    touch_injector_->set_display_mode(mode);
  }

  display_mode_ = mode;
}

void DisplayOverlayController::SetDisplayMode(DisplayMode mode) {
  if (display_mode_ == mode) {
    return;
  }

  switch (mode) {
    case DisplayMode::kNone:
      RemoveAllWidgets();
      break;

    case DisplayMode::kView: {
      DCHECK(!rich_nudge_widget_);
      DCHECK(!target_widget_);
      DCHECK(!button_options_widget_);
      RemoveActionHighlightWidget();
      RemoveDeleteEditShortcutWidget();
      RemoveEditingListWidget();
      RemoveFocusCycler();
      if (GetActiveActionsSize() == 0u) {
        // If there is no active action in `kView` mode, it doesn't create
        // `input_mapping_widget_` to save resources. When switching from
        // `kEdit` mode, destroy `input_mapping_widget_` for no active actions.
        RemoveInputMappingWidget();
      } else {
        AddInputMappingWidget();
        if (auto* input_mapping = GetInputMapping()) {
          input_mapping->SetDisplayMode(mode);
        }
        SetInputMappingVisible(
            /*visible=*/touch_injector_->input_mapping_visible());

        // In the view mode, make sure the input mapping is displayed under game
        // dashboard UIs.
        StackInputMappingAtBottomForViewMode();

        auto* input_mapping_window = input_mapping_widget_->GetNativeWindow();
        input_mapping_window->SetEventTargetingPolicy(
            aura::EventTargetingPolicy::kNone);
      }
    } break;

    case DisplayMode::kEdit: {
      if (GetActiveActionsSize() == 0u) {
        // Because`input_mapping_widget_` was not created in `kView` mode,
        // create `input_mapping_widget_` in `kEdit` mode for adding new
        // actions.
        AddInputMappingWidget();
      }

      AddFocusCycler();

      // No matter if the mapping hint is hidden, `input_mapping_widget_` needs
      // to show up in `kEdit` mode.
      SetInputMappingVisible(/*visible=*/true);

      // Since `focus_cycler_` was added in `kEdit` mode after
      // `input_mapping_widget_` in general. Refresh `input_mapping_widget_` to
      // make sure it is added in `focus_cycler_`.
      focus_cycler_->RefreshWidget(input_mapping_widget_.get());

      if (auto* input_mapping = GetInputMapping()) {
        input_mapping->SetDisplayMode(mode);
      }
      auto* input_mapping_window = input_mapping_widget_->GetNativeWindow();
      input_mapping_window->SetEventTargetingPolicy(
          aura::EventTargetingPolicy::kTargetAndDescendants);
      AddEditingListWidget();
    } break;

    default:
      break;
  }

  display_mode_ = mode;
}

std::optional<gfx::Rect> DisplayOverlayController::GetOverlayMenuEntryBounds() {
  if (!menu_entry_ || !menu_entry_->GetVisible()) {
    return std::nullopt;
  }

  return std::optional<gfx::Rect>(menu_entry_->GetBoundsInScreen());
}

void DisplayOverlayController::AddEditMessage(std::string_view message,
                                              MessageType message_type) {
  // No need to show edit message for Beta version.
  // There is no instance for unittest.
  if (IsBeta() || !ash::Shell::HasInstance()) {
    return;
  }

  RemoveEditMessage();
  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  if (!overlay_widget) {
    return;
  }
  auto* parent_view = overlay_widget->GetContentsView();
  DCHECK(parent_view);
  if (!parent_view) {
    return;
  }
  message_ = MessageView::Show(this, parent_view, message, message_type);
}

void DisplayOverlayController::RemoveEditMessage() {
  if (!message_) {
    return;
  }
  message_->parent()->RemoveChildViewT(message_);
  message_ = nullptr;
}

void DisplayOverlayController::OnInputBindingChange(
    Action* action,
    std::unique_ptr<InputElement> input_element) {
  touch_injector_->OnInputBindingChange(action, std::move(input_element));
}

void DisplayOverlayController::SaveToProtoFile() {
  touch_injector_->OnSaveProtoFile();
}

void DisplayOverlayController::OnCustomizeSave() {
  touch_injector_->OnBindingSave();
  if (IsBeta()) {
    SetDisplayMode(DisplayMode::kView);
  } else {
    SetDisplayModeAlpha(DisplayMode::kView);
  }
}

void DisplayOverlayController::OnCustomizeCancel() {
  touch_injector_->OnBindingCancel();
}

void DisplayOverlayController::OnCustomizeRestore() {
  touch_injector_->OnBindingRestore();
}

const std::string& DisplayOverlayController::GetPackageName() const {
  return touch_injector_->package_name();
}

void DisplayOverlayController::OnApplyMenuState() {
  if (display_mode_ != DisplayMode::kView) {
    return;
  }

  SetInputMappingVisible(
      /*visible=*/GetTouchInjectorEnable() && GetInputMappingViewVisible(),
      /*store_visible_state=*/true);
}

InputOverlayWindowStateType DisplayOverlayController::GetWindowStateType()
    const {
  DCHECK(touch_injector_);
  auto* window = touch_injector_->window();
  DCHECK(window);
  auto* state = ash::WindowState::Get(window);
  InputOverlayWindowStateType type = InputOverlayWindowStateType::kInvalid;
  if (state) {
    if (state->IsNormalStateType()) {
      type = InputOverlayWindowStateType::kNormal;
    } else if (state->IsMaximized()) {
      type = InputOverlayWindowStateType::kMaximized;
    } else if (state->IsFullscreen()) {
      type = InputOverlayWindowStateType::kFullscreen;
    } else if (state->IsSnapped()) {
      type = InputOverlayWindowStateType::kSnapped;
    }
  }
  return type;
}

void DisplayOverlayController::AddNewAction(ActionType action_type,
                                            const gfx::Point& target_pos) {
  // Keep adding new action before exiting button placement mode because
  // `target_pos` is from button placement mode.
  touch_injector_->AddNewAction(action_type, target_pos);
  ExitButtonPlaceMode(/*is_action_added=*/true);
}

void DisplayOverlayController::RemoveAction(Action* action) {
  // TODO(b/270973654): Show delete confirmation dialog here.
  touch_injector_->RemoveAction(action);
}

void DisplayOverlayController::RemoveActionNewState(Action* action) {
  if (!action->is_new()) {
    return;
  }
  touch_injector_->RemoveActionNewState(action);
}

size_t DisplayOverlayController::GetActiveActionsSize() const {
  return touch_injector_->GetActiveActionsSize();
}

bool DisplayOverlayController::HasSingleUserAddedAction() const {
  return touch_injector_->HasSingleUserAddedAction();
}

bool DisplayOverlayController::IsActiveAction(Action* action) const {
  const auto& actions = touch_injector_->actions();
  if (actions.empty()) {
    return false;
  }

  auto it = std::find_if(
      actions.begin(), actions.end(),
      [&](const std::unique_ptr<Action>& p) { return action == p.get(); });
  return it != actions.end() && !(it->get()->IsDeleted());
}

MappingSource DisplayOverlayController::GetMappingSource() const {
  const auto& actions = touch_injector_->actions();
  if (actions.empty()) {
    return MappingSource::kEmpty;
  }

  // Check if there is any default action.
  auto default_it = std::find_if(
      actions.begin(), actions.end(),
      [&](const std::unique_ptr<Action>& p) { return p->IsDefaultAction(); });

  // Check if there is any user added action.
  auto user_added_it = std::find_if(
      actions.begin(), actions.end(),
      [&](const std::unique_ptr<Action>& p) { return !p->IsDefaultAction(); });

  return default_it != actions.end() && user_added_it != actions.end()
             ? MappingSource::kDefaultAndUserAdded
             : (default_it != actions.end() ? MappingSource::kDefault
                                            : MappingSource::kUserAdded);
}

void DisplayOverlayController::AddTouchInjectorObserver(
    TouchInjectorObserver* observer) {
  touch_injector_->AddObserver(observer);
}

void DisplayOverlayController::RemoveTouchInjectorObserver(
    TouchInjectorObserver* observer) {
  touch_injector_->RemoveObserver(observer);
}

void DisplayOverlayController::AddButtonOptionsMenuWidget(Action* action) {
  if (!IsBeta()) {
    return;
  }

  if (button_options_widget_) {
    auto* menu = GetButtonOptionsMenu();
    if (menu && menu->action() == action) {
      return;
    }
    RemoveButtonOptionsMenuWidget();
  }

  button_options_widget_ =
      CreateTransientWidget(input_mapping_widget_->GetNativeWindow(),
                            /*widget_name=*/kButtonOptionsMenu,
                            /*accept_events=*/true);
  widget_observations_.AddObservation(button_options_widget_.get());
  button_options_widget_->SetContentsView(
      std::make_unique<ButtonOptionsMenu>(this, action));
  UpdateButtonOptionsMenuWidgetBounds();
  button_options_widget_->widget_delegate()->SetAccessibleTitle(
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_BUTTON_OPTIONS_A11Y_LABEL));

  // Always hide editing list when button options menu shows up.
  SetEditingListVisibility(/*visible=*/false);

  button_options_widget_->Show();
}

void DisplayOverlayController::RemoveButtonOptionsMenuWidget() {
  if (!button_options_widget_) {
    return;
  }
  // Check if related action is already deleted.
  if (const auto* menu = GetButtonOptionsMenu()) {
    if (auto* menu_action = menu->action(); IsActiveAction(menu_action)) {
      RemoveActionNewState(menu_action);
    }
  }

  button_options_widget_->Close();
  button_options_widget_.reset();
}

void DisplayOverlayController::SetButtonOptionsMenuWidgetVisibility(
    bool is_visible) {
  if (!button_options_widget_) {
    return;
  }

  if (is_visible) {
    if (GetButtonOptionsMenu()) {
      UpdateButtonOptionsMenuWidgetBounds();
    }
    button_options_widget_->Show();
  } else {
    button_options_widget_->Hide();
  }
}

void DisplayOverlayController::AddDeleteEditShortcutWidget(
    ActionViewListItem* anchor_view) {
  if (!delete_edit_shortcut_widget_) {
    delete_edit_shortcut_widget_ =
        base::WrapUnique(views::BubbleDialogDelegateView::CreateBubble(
            std::make_unique<DeleteEditShortcut>(this, anchor_view)));
    widget_observations_.AddObservation(delete_edit_shortcut_widget_.get());
  }

  if (auto* shortcut = GetDeleteEditShortcut();
      shortcut->GetAnchorView() != anchor_view) {
    shortcut->UpdateAnchorView(anchor_view);
  }

  delete_edit_shortcut_widget_->Show();
}

void DisplayOverlayController::RemoveDeleteEditShortcutWidget() {
  if (delete_edit_shortcut_widget_) {
    delete_edit_shortcut_widget_->Close();
    delete_edit_shortcut_widget_.reset();
  }
}

void DisplayOverlayController::AddActionHighlightWidget(Action* action) {
  auto* anchor_view = action->action_view();
  if (!action_highlight_widget_) {
    action_highlight_widget_ = CreateTransientWidget(
        touch_injector_->window(), /*widget_name=*/kActionHighlight,
        /*accept_events=*/false);
    action_highlight_widget_->SetContentsView(
        std::make_unique<ActionHighlight>(this, anchor_view));
  }

  if (auto* highlight = views::AsViewClass<ActionHighlight>(
          action_highlight_widget_->GetContentsView())) {
    highlight->UpdateAnchorView(anchor_view);

    // Show `action_highlight_widget_` if it is hidden.
    if (!action_highlight_widget_->IsVisible()) {
      action_highlight_widget_->ShowInactive();
      input_mapping_widget_->StackAboveWidget(action_highlight_widget_.get());
    }
  }
}

void DisplayOverlayController::RemoveActionHighlightWidget() {
  if (action_highlight_widget_) {
    action_highlight_widget_->Close();
    action_highlight_widget_.reset();
  }
}

void DisplayOverlayController::HideActionHighlightWidget() {
  if (action_highlight_widget_) {
    action_highlight_widget_->Hide();
  }
}

void DisplayOverlayController::HideActionHighlightWidgetForAction(
    Action* action) {
  if (!action_highlight_widget_) {
    return;
  }

  if (auto* highlight = views::AsViewClass<ActionHighlight>(
          action_highlight_widget_->GetContentsView());
      highlight && highlight->anchor_view() == action->action_view()) {
    action_highlight_widget_->Hide();
  }
}

void DisplayOverlayController::UpdateWidgetBoundsInRootWindow(
    views::Widget* widget,
    const gfx::Rect& bounds_in_root_window) {
  auto root_bounds =
      touch_injector_->window()->GetRootWindow()->GetBoundsInScreen();
  auto bounds_in_screen = bounds_in_root_window;
  bounds_in_screen.Offset(root_bounds.OffsetFromOrigin());
  widget->SetBounds(bounds_in_screen);
}

ActionViewListItem* DisplayOverlayController::GetEditingListItemForAction(
    Action* action) {
  if (auto* editing_list = GetEditingList()) {
    return editing_list->GetListItemForAction(action);
  }
  return nullptr;
}

void DisplayOverlayController::OnMouseEvent(ui::MouseEvent* event) {
  if ((display_mode_ == DisplayMode::kView && IsNudgeEmpty()) ||
      event->type() != ui::EventType::kMousePressed) {
    return;
  }

  ProcessPressedEvent(*event);
}

void DisplayOverlayController::OnTouchEvent(ui::TouchEvent* event) {
  if ((display_mode_ == DisplayMode::kView && IsNudgeEmpty()) ||
      event->type() != ui::EventType::kTouchPressed) {
    return;
  }
  ProcessPressedEvent(*event);
}

void DisplayOverlayController::OnKeyEvent(ui::KeyEvent* event) {
  if (focus_cycler_) {
    focus_cycler_->MaybeChangeFocusWidget(*event);
  }
}

void DisplayOverlayController::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {
  DCHECK_EQ(window, touch_injector_->window());
  // Disregard the bounds from animation and only care final window bounds.
  if (reason == ui::PropertyChangeReason::FROM_ANIMATION) {
    return;
  }

  UpdateForBoundsChanged();
}

void DisplayOverlayController::OnWindowPropertyChanged(aura::Window* window,
                                                       const void* key,
                                                       intptr_t old) {
  DCHECK_EQ(window, touch_injector_->window());
  if (key == chromeos::kImmersiveIsActive) {
    bool is_immersive = window->GetProperty(chromeos::kImmersiveIsActive);
    // This is to catch the corner case that when an app is launched as
    // fullscreen/immersive mode, so it only cares when the window turns into
    // immersive mode from non-immersive mode.
    if (!is_immersive || is_immersive == static_cast<bool>(old)) {
      return;
    }

    UpdateForBoundsChanged();
  }

  if (IsBeta() && key == ash::kArcGameControlsFlagsKey) {
    ash::ArcGameControlsFlag old_flags =
        static_cast<ash::ArcGameControlsFlag>(old);
    ash::ArcGameControlsFlag flags =
        window->GetProperty(ash::kArcGameControlsFlagsKey);
    if (flags != old_flags) {
      UpdateDisplayMode();

      SetTouchInjectorEnable(
          IsFlagSet(flags, ash::ArcGameControlsFlag::kEnabled));

      // `input_mapping_widget_` is always visible in edit mode.
      SetInputMappingVisible(/*visible=*/
                             IsFlagSet(flags, ash::ArcGameControlsFlag::kEdit)
                                 ? true
                                 : IsFlagSet(flags,
                                             ash::ArcGameControlsFlag::kHint),
                             /*store_visible_state=*/true);

      // Save the menu states upon menu closing.
      if (IsFlagChanged(flags, old_flags, ash::ArcGameControlsFlag::kMenu) &&
          !IsFlagSet(flags, ash::ArcGameControlsFlag::kMenu)) {
        touch_injector_->OnInputMenuViewRemoved();
      }

      UpdateEventRewriteCapability();

      // Record metrics.
      const auto mapping_source = GetMappingSource();
      if (IsFlagChanged(flags, old_flags, ash::ArcGameControlsFlag::kEnabled)) {
        RecordToggleWithMappingSource(
            GetPackageName(),
            /*is_feature=*/true,
            /*is_on=*/IsFlagSet(flags, ash::ArcGameControlsFlag::kEnabled),
            mapping_source);
      }
      if (IsFlagChanged(flags, old_flags, ash::ArcGameControlsFlag::kHint)) {
        RecordToggleWithMappingSource(
            GetPackageName(),
            /*is_feature=*/false,
            /*is_on=*/IsFlagSet(flags, ash::ArcGameControlsFlag::kHint),
            mapping_source);
      }
    }
  }
}

void DisplayOverlayController::OnWidgetVisibilityChanged(views::Widget* widget,
                                                         bool visible) {
  if (focus_cycler_) {
    focus_cycler_->RefreshWidget(widget);
  }
}

void DisplayOverlayController::OnWidgetDestroying(views::Widget* widget) {
  widget_observations_.RemoveObservation(widget);
}

bool DisplayOverlayController::HasMenuView() const {
  return input_menu_view_ != nullptr;
}

void DisplayOverlayController::SetInputMappingVisible(
    bool visible,
    bool store_visible_state) {
  // There is no `input_mapping_widget_` or `input_mapping_view_` if there is no
  // active action or the feature is disabled.
  if (IsBeta() && input_mapping_widget_ &&
      input_mapping_widget_->IsVisible() != visible) {
    if (visible) {
      input_mapping_widget_->ShowInactive();
    } else {
      input_mapping_widget_->Hide();
    }
  } else if (!IsBeta() && input_mapping_view_) {
    input_mapping_view_->SetVisible(visible);
  }

  if (store_visible_state) {
    CHECK(touch_injector_);
    touch_injector_->store_input_mapping_visible(visible);
  }
}

bool DisplayOverlayController::GetInputMappingViewVisible() const {
  DCHECK(touch_injector_);
  if (!touch_injector_) {
    return false;
  }
  return touch_injector_->input_mapping_visible();
}

void DisplayOverlayController::SetTouchInjectorEnable(bool enable) {
  DCHECK(touch_injector_);
  if (!touch_injector_) {
    return;
  }
  touch_injector_->store_touch_injector_enable(enable);
}

bool DisplayOverlayController::GetTouchInjectorEnable() {
  DCHECK(touch_injector_);
  if (!touch_injector_) {
    return false;
  }
  return touch_injector_->touch_injector_enable();
}

void DisplayOverlayController::ProcessPressedEvent(
    const ui::LocatedEvent& event) {
  if (IsBeta()) {
    if (auto* delete_edit_view = GetDeleteEditShortcut()) {
      if (const auto bounds = delete_edit_view->GetBoundsInScreen();
          !bounds.Contains(event.root_location())) {
        RemoveDeleteEditShortcutWidget();
      }
    }
  } else {
    if (!message_ && !input_menu_view_ && !nudge_view_) {
      return;
    }

    auto root_location = event.root_location();
    // Convert the LocatedEvent root location to screen location.
    auto origin = touch_injector_->window()
                      ->GetRootWindow()
                      ->GetBoundsInScreen()
                      .origin();
    root_location.Offset(origin.x(), origin.y());

    if (message_ && !message_->GetBoundsInScreen().Contains(root_location)) {
      RemoveEditMessage();
    }

    if (input_menu_view_ &&
        !input_menu_view_->GetBoundsInScreen().Contains(root_location)) {
      SetDisplayModeAlpha(DisplayMode::kView);
    }

    // Dismiss the nudge, regardless where the click was.
    if (nudge_view_) {
      OnNudgeDismissed();
    }
  }
}

void DisplayOverlayController::SetMenuEntryHoverState(bool curr_hover_state) {
  if (menu_entry_) {
    menu_entry_->ChangeHoverState(curr_hover_state);
  }
}

void DisplayOverlayController::EnsureTaskWindowToFrontForViewMode(
    views::Widget* overlay_widget) {
  DCHECK(overlay_widget);
  DCHECK(overlay_widget->GetNativeWindow());
  DCHECK_EQ(overlay_widget->GetNativeWindow()->event_targeting_policy(),
            aura::EventTargetingPolicy::kNone);

  auto* shell_surface_base =
      exo::GetShellSurfaceBaseForWindow(touch_injector_->window());
  DCHECK(shell_surface_base);
  auto* host_window = shell_surface_base->host_window();
  DCHECK(host_window);
  if (const auto& children = host_window->children(); children.size() > 0u) {
    // First child is the root ExoSurface window. Focus on the root surface
    // window can bring the task window to the front of the task stack.
    if (!children[0]->HasFocus()) {
      children[0]->Focus();
    }
  } else {
    host_window->Focus();
  }
}

void DisplayOverlayController::UpdateForBoundsChanged() {
  auto content_bounds = CalculateWindowContentBounds(touch_injector_->window());
  if (content_bounds == touch_injector_->content_bounds_f()) {
    return;
  }
  touch_injector_->UpdateForOverlayBoundsChanged(content_bounds);

  if (IsBeta()) {
    UpdateInputMappingWidgetBounds();
    UpdateEditingListWidgetBounds();
    UpdateTargetWidgetBounds();
    UpdateButtonOptionsMenuWidgetBounds();
  } else {
    // Overlay widget is null for test.
    if (!GetOverlayWidget()) {
      return;
    }

    auto mode = display_mode_;
    SetDisplayModeAlpha(DisplayMode::kNone);
    // Transition to `kView` mode except while on `kEducation` mode since the
    // educational banner needs to remain visible until dismissed by the user.
    if (mode != DisplayMode::kEducation) {
      mode = DisplayMode::kView;
    }

    SetDisplayModeAlpha(mode);
  }
}

void DisplayOverlayController::RemoveAllWidgets() {
  RemoveRichNudge();
  RemoveDeleteEditShortcutWidget();
  RemoveTargetWidget();
  RemoveButtonOptionsMenuWidget();
  RemoveEditingListWidget();
  RemoveActionHighlightWidget();
  RemoveInputMappingWidget();
}

void DisplayOverlayController::AddInputMappingWidget() {
  if (input_mapping_widget_) {
    return;
  }

  input_mapping_widget_ = CreateTransientWidget(touch_injector_->window(),
                                                /*widget_name=*/kInputMapping,
                                                /*accept_events=*/false);
  widget_observations_.AddObservation(input_mapping_widget_.get());
  input_mapping_widget_->SetContentsView(
      std::make_unique<InputMappingView>(this));
  UpdateInputMappingWidgetBounds();
}

void DisplayOverlayController::RemoveInputMappingWidget() {
  if (input_mapping_widget_) {
    input_mapping_widget_->Close();
    input_mapping_widget_.reset();
  }
}

InputMappingView* DisplayOverlayController::GetInputMapping() {
  if (!input_mapping_widget_) {
    return nullptr;
  }

  return views::AsViewClass<InputMappingView>(
      input_mapping_widget_->GetContentsView());
}

void DisplayOverlayController::StackInputMappingAtBottomForViewMode() {
  if (!input_mapping_widget_ || display_mode_ != DisplayMode::kView) {
    return;
  }

  input_mapping_widget_->Deactivate();

  // ash::GameDashboardController::Get() is empty for the
  // unit test.
  if (auto* gd_controller = ash::GameDashboardController::Get()) {
    gd_controller->MaybeStackAboveWidget(touch_injector_->window(),
                                         input_mapping_widget_.get());
  }
}

void DisplayOverlayController::AddEditingListWidget() {
  if (editing_list_widget_) {
    return;
  }
  editing_list_widget_ = CreateTransientWidget(
      input_mapping_widget_->GetNativeWindow(), /*widget_name=*/kEditingList,
      /*accept_events=*/true);
  widget_observations_.AddObservation(editing_list_widget_.get());
  editing_list_widget_->SetContentsView(std::make_unique<EditingList>(this));

  // Avoid active conflict with the game dashboard main menu.
  editing_list_widget_->ShowInactive();
  UpdateEditingListWidgetBounds();
  editing_list_widget_->widget_delegate()->SetAccessibleTitle(
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_EDITING_LIST_A11Y_LABEL));
}

void DisplayOverlayController::RemoveEditingListWidget() {
  if (editing_list_widget_) {
    editing_list_widget_->Close();
    editing_list_widget_.reset();

    UpdateFlagAndProperty(touch_injector_->window(),
                          ash::ArcGameControlsFlag::kEdit,
                          /*turn_on=*/false);
    UpdateEventRewriteCapability();
  }
}

void DisplayOverlayController::SetEditingListVisibility(bool visible) {
  if (!editing_list_widget_ || editing_list_widget_->IsVisible() == visible) {
    return;
  }

  if (visible) {
    editing_list_widget_->Show();
  } else {
    editing_list_widget_->Hide();
  }
}

EditingList* DisplayOverlayController::GetEditingList() {
  if (!editing_list_widget_) {
    return nullptr;
  }

  return views::AsViewClass<EditingList>(
      editing_list_widget_->GetContentsView());
}

ButtonOptionsMenu* DisplayOverlayController::GetButtonOptionsMenu() {
  if (!button_options_widget_) {
    return nullptr;
  }

  return views::AsViewClass<ButtonOptionsMenu>(
      button_options_widget_->GetContentsView());
}

void DisplayOverlayController::AddFocusCycler() {
  if (!focus_cycler_) {
    focus_cycler_ = std::make_unique<FocusCycler>();
  }
}

void DisplayOverlayController::RemoveFocusCycler() {
  if (focus_cycler_) {
    focus_cycler_.reset();
  }
}

void DisplayOverlayController::EnterButtonPlaceMode(ActionType action_type) {
  RemoveDeleteEditShortcutWidget();
  SetEditingListVisibility(/*visible=*/false);
  AddTargetWidget(action_type);
  AddRichNudge();
}

void DisplayOverlayController::ExitButtonPlaceMode(bool is_action_added) {
  RemoveRichNudge();
  RemoveTargetWidget();
  if (!is_action_added) {
    SetEditingListVisibility(/*visible=*/true);
  }
}

void DisplayOverlayController::UpdateButtonPlacementNudgeAnchorRect() {
  auto* target_view = GetTargetView();
  if (!target_view) {
    return;
  }

  auto target_circle_bounds = target_view->GetTargetCircleBounds();
  views::View::ConvertRectToScreen(target_view, &target_circle_bounds);
  auto* rich_nudge = GetRichNudge();
  if (rich_nudge && rich_nudge_widget_->GetWindowBoundsInScreen().Intersects(
                        target_circle_bounds)) {
    rich_nudge->FlipPosition();
  }
}

void DisplayOverlayController::AddTargetWidget(ActionType action_type) {
  DCHECK(!target_widget_);

  target_widget_ = CreateTransientWidget(touch_injector_->window(),
                                         /*widget_name=*/kInputMapping,
                                         /*accept_events=*/true);
  target_widget_->SetContentsView(
      std::make_unique<TargetView>(this, action_type));
  target_widget_->ShowInactive();
}

void DisplayOverlayController::RemoveTargetWidget() {
  if (target_widget_) {
    target_widget_->Close();
    target_widget_.reset();
  }
}

TargetView* DisplayOverlayController::GetTargetView() const {
  return target_widget_
             ? views::AsViewClass<TargetView>(target_widget_->GetContentsView())
             : nullptr;
}

void DisplayOverlayController::AddRichNudge() {
  if (GetTargetView()) {
    rich_nudge_widget_ =
        base::WrapUnique(views::BubbleDialogDelegateView::CreateBubble(
            std::make_unique<RichNudge>(target_widget_->GetNativeWindow())));
    rich_nudge_widget_->ShowInactive();
  }
}

void DisplayOverlayController::RemoveRichNudge() {
  if (rich_nudge_widget_) {
    rich_nudge_widget_->Close();
    rich_nudge_widget_.reset();
  }
}

RichNudge* DisplayOverlayController::GetRichNudge() const {
  return rich_nudge_widget_ ? views::AsViewClass<RichNudge>(
                                  rich_nudge_widget_->widget_delegate()
                                      ->AsBubbleDialogDelegate()
                                      ->GetContentsView())
                            : nullptr;
}

DeleteEditShortcut* DisplayOverlayController::GetDeleteEditShortcut() const {
  return delete_edit_shortcut_widget_
             ? views::AsViewClass<DeleteEditShortcut>(
                   delete_edit_shortcut_widget_->widget_delegate()
                       ->AsBubbleDialogDelegate()
                       ->GetContentsView())
             : nullptr;
}

void DisplayOverlayController::UpdateButtonOptionsMenuWidgetBounds() {
  // There is no `button_options_widget_` in view mode.
  if (!button_options_widget_) {
    return;
  }

  if (auto* menu = GetButtonOptionsMenu()) {
    menu->UpdateWidget();
  }
}

void DisplayOverlayController::UpdateInputMappingWidgetBounds() {
  // There is no `input_mapping_widget_` if there is no active action or gio is
  // disabled.
  if (!input_mapping_widget_) {
    return;
  }

  UpdateWidgetBoundsInRootWindow(input_mapping_widget_.get(),
                                 touch_injector_->content_bounds());
  StackInputMappingAtBottomForViewMode();
}

void DisplayOverlayController::UpdateEditingListWidgetBounds() {
  // There is no `editing_list_widget_` in view mode.
  if (!editing_list_widget_) {
    return;
  }

  if (auto* editing_list = GetEditingList()) {
    editing_list->UpdateWidget();
  }
}

void DisplayOverlayController::UpdateTargetWidgetBounds() {
  if (!target_widget_) {
    return;
  }

  if (auto* target_view = GetTargetView()) {
    target_view->UpdateWidgetBounds();
  }
}

void DisplayOverlayController::UpdateEventRewriteCapability() {
  ash::ArcGameControlsFlag flags =
      touch_injector_->window()->GetProperty(ash::kArcGameControlsFlagsKey);

  touch_injector_->set_can_rewrite_event(
      IsFlagSet(flags, ash::ArcGameControlsFlag::kEnabled) &&
      !IsFlagSet(flags, ash::ArcGameControlsFlag::kEmpty) &&
      !IsFlagSet(flags, ash::ArcGameControlsFlag::kMenu) &&
      !IsFlagSet(flags, ash::ArcGameControlsFlag::kEdit));
}

void DisplayOverlayController::DismissEducationalViewForTesting() {
  OnEducationalViewDismissed();
}

}  // namespace arc::input_overlay
