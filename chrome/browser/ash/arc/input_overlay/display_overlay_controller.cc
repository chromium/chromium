// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"

#include <memory>

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/game_dashboard/game_dashboard_utils.h"
#include "ash/game_dashboard/game_dashboard_widget.h"
#include "ash/public/cpp/arc_game_controls_flag.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/style/style_util.h"
#include "ash/wm/window_state.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/button_label_list.h"
#include "chrome/browser/ash/arc/input_overlay/ui/button_options_menu.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_finish_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/editing_list.h"
#include "chrome/browser/ash/arc/input_overlay/ui/educational_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_menu_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/menu_entry_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/message_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/nudge_view.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "ui/views/background.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace arc::input_overlay {

namespace {
// UI specs.
constexpr int kMenuEntrySideMargin = 24;
constexpr int kNudgeVerticalAlign = 8;

constexpr char kButtonLabelList[] = "GameControlsButtonLabelList";
constexpr char kButtonOptionsMenu[] = "GameControlsButtonOptionsMenu";
constexpr char kEditingList[] = "GameControlsEditingList";
constexpr char kInputMapping[] = "GameControlsInputMapping";

std::unique_ptr<ash::GameDashboardWidget> CreateTransientWidget(
    aura::Window* parent_window,
    const std::string& widget_name,
    bool accept_events,
    bool is_floating) {
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = parent_window;
  params.name = widget_name;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.activatable = views::Widget::InitParams::Activatable::kYes;
  params.accept_events = accept_events;

  auto widget = std::make_unique<ash::GameDashboardWidget>();
  widget->Init(std::move(params));

  auto* widget_window = widget->GetNativeWindow();
  DCHECK_EQ(parent_window, wm::GetTransientParent(widget_window));
  wm::TransientWindowManager::GetOrCreate(widget_window)
      ->set_parent_controls_visibility(false);
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_NONE);
  if (is_floating) {
    widget->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
  }
  return widget;
}

}  // namespace

DisplayOverlayController::DisplayOverlayController(
    TouchInjector* touch_injector,
    bool first_launch)
    : touch_injector_(touch_injector) {
  touch_injector_->set_display_overlay_controller(this);

  if (IsBeta()) {
    auto* window = touch_injector_->window();
    window->AddObserver(this);
    SetDisplayMode(ash::game_dashboard_utils::IsFlagSet(
                       window->GetProperty(ash::kArcGameControlsFlagsKey),
                       ash::ArcGameControlsFlag::kEdit)
                       ? DisplayMode::kEdit
                       : DisplayMode::kView);
  } else {
    // There is no instance for unittest.
    if (!ash::Shell::HasInstance()) {
      return;
    }

    AddOverlay(first_launch ? DisplayMode::kEducation : DisplayMode::kView);
    ash::Shell::Get()->AddPreTargetHandler(this);
  }
}

DisplayOverlayController::~DisplayOverlayController() {
  touch_injector_->set_display_overlay_controller(nullptr);

  if (IsBeta()) {
    touch_injector_->window()->RemoveObserver(this);
    RemoveAllWidgets();
  } else {
    // There is no instance for unittest.
    if (!ash::Shell::HasInstance()) {
      return;
    }

    ash::Shell::Get()->RemovePreTargetHandler(this);
    RemoveOverlayIfAny();
  }
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
  auto* shell_surface_base =
      exo::GetShellSurfaceBaseForWindow(touch_injector_->window());
  if (shell_surface_base && shell_surface_base->HasOverlay()) {
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
    absl::optional<gfx::Point> location) {
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
  SetInputMappingVisible(touch_injector_->input_mapping_visible());
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
  auto* shell_surface_base =
      exo::GetShellSurfaceBaseForWindow(touch_injector_->window());
  // Shell surface is null for test.
  if (!shell_surface_base) {
    return nullptr;
  }

  return shell_surface_base ? static_cast<views::Widget*>(
                                  shell_surface_base->GetFocusTraversable())
                            : nullptr;
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
      NOTREACHED();
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
  ash::ArcGameControlsFlag flags =
      touch_injector_->window()->GetProperty(ash::kArcGameControlsFlagsKey);

  // If GIO is empty or disabled in the view mode , it doesn't create any
  // widgets to save resources.
  if (!ash::game_dashboard_utils::IsFlagSet(flags,
                                            ash::ArcGameControlsFlag::kEdit) &&
      (!ash::game_dashboard_utils::IsFlagSet(
           flags, ash::ArcGameControlsFlag::kEnabled) ||
       ash::game_dashboard_utils::IsFlagSet(
           flags, ash::ArcGameControlsFlag::kEmpty))) {
    return;
  }

  AddInputMappingWidget();
  auto* input_mapping_view =
      static_cast<InputMappingView*>(input_mapping_widget_->GetContentsView());
  input_mapping_view->SetDisplayMode(mode);
  auto* input_mapping_window = input_mapping_widget_->GetNativeWindow();
  switch (mode) {
    case DisplayMode::kView:
      input_mapping_window->SetEventTargetingPolicy(
          aura::EventTargetingPolicy::kNone);
      RemoveButtonOptionsMenuWidget();
      RemoveEditingListWidget();
      break;
    case DisplayMode::kEdit:
      input_mapping_window->SetEventTargetingPolicy(
          aura::EventTargetingPolicy::kTargetAndDescendants);
      AddEditingListWidget();
      break;
    default:
      break;
  }
}

void DisplayOverlayController::TurnFlag(ash::ArcGameControlsFlag flag,
                                        bool turn_on) {
  auto* window = touch_injector_->window();
  const ash::ArcGameControlsFlag flags =
      window->GetProperty(ash::kArcGameControlsFlagsKey);
  window->SetProperty(
      ash::kArcGameControlsFlagsKey,
      ash::game_dashboard_utils::UpdateFlag(flags, flag, turn_on));
}

absl::optional<gfx::Rect>
DisplayOverlayController::GetOverlayMenuEntryBounds() {
  if (!menu_entry_ || !menu_entry_->GetVisible()) {
    return absl::nullopt;
  }

  return absl::optional<gfx::Rect>(menu_entry_->GetBoundsInScreen());
}

void DisplayOverlayController::AddEditMessage(const base::StringPiece& message,
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

  SetInputMappingVisible(GetTouchInjectorEnable() &&
                         GetInputMappingViewVisible());
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

void DisplayOverlayController::AddNewAction(ActionType action_type) {
  touch_injector_->AddNewAction(action_type);
}

void DisplayOverlayController::RemoveAction(Action* action) {
  // TODO(b/270973654): Show delete confirmation dialog here.
  touch_injector_->RemoveAction(action);
}

void DisplayOverlayController::ChangeActionName(Action* action,
                                                std::u16string name) {
  touch_injector_->ChangeActionName(action, name);
}

size_t DisplayOverlayController::GetActiveActionsSize() {
  size_t active_size = 0;
  for (auto& action : touch_injector_->actions()) {
    if (!action->IsDeleted()) {
      active_size++;
    }
  }
  return active_size;
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
    auto* menu = static_cast<ButtonOptionsMenu*>(
        button_options_widget_->GetContentsView());
    if (menu->action() == action) {
      return;
    }
    RemoveButtonOptionsMenuWidget();
  }

  button_options_widget_ = CreateTransientWidget(
      touch_injector_->window(), /*widget_name=*/kButtonOptionsMenu,
      /*accept_events=*/true, /*is_floating=*/true);
  button_options_widget_->SetContentsView(
      std::make_unique<ButtonOptionsMenu>(this, action));
  UpdateButtonOptionsMenuWidgetBounds(action);

  button_options_widget_->Show();
}

void DisplayOverlayController::RemoveButtonOptionsMenuWidget() {
  if (button_options_widget_) {
    button_options_widget_->Close();
    button_options_widget_.reset();

    RemoveButtonLabelListWidget();
  }
}

void DisplayOverlayController::OnButtonOptionsMenuButtonLabelPressed(
    Action* action) {
  DCHECK(button_options_widget_);
  button_options_widget_->Hide();
  AddButtonLabelListWidget(action);
}

void DisplayOverlayController::AddButtonLabelListWidget(Action* action) {
  if (button_label_list_widget_) {
    return;
  }

  button_label_list_widget_ = CreateTransientWidget(
      touch_injector_->window(), /*widget_name=*/kButtonLabelList,
      /*accept_events=*/true, /*is_floating=*/true);
  auto* view = button_label_list_widget_->SetContentsView(
      std::make_unique<ButtonLabelList>(this, action));
  auto* window = button_label_list_widget_->GetNativeWindow();
  window->parent()->StackChildAtTop(window);
  UpdateWidgetBoundsInRootWindow(
      button_label_list_widget_.get(),
      gfx::Rect(action->action_view()->CalculateAttachViewPositionInRootWindow(
                    touch_injector_->window()->GetRootWindow()->bounds(),
                    touch_injector_->content_bounds().origin(), view),
                view->GetPreferredSize()));
  button_label_list_widget_->Show();
}

void DisplayOverlayController::RemoveButtonLabelListWidget() {
  if (!button_label_list_widget_) {
    return;
  }
  button_label_list_widget_->Close();
  button_label_list_widget_.reset();
}

void DisplayOverlayController::OnButtonLabelListBackButtonPressed() {
  RemoveButtonLabelListWidget();
  button_options_widget_->Show();
}

void DisplayOverlayController::UpdateButtonOptionsMenuWidgetBounds(
    Action* action) {
  if (!button_options_widget_) {
    return;
  }

  auto* menu =
      static_cast<ArrowContainer*>(button_options_widget_->GetContentsView());
  UpdateWidgetBoundsInRootWindow(
      button_options_widget_.get(),
      gfx::Rect(action->action_view()->CalculateAttachViewPositionInRootWindow(
                    touch_injector_->window()->GetRootWindow()->bounds(),
                    touch_injector_->content_bounds().origin(), menu),
                menu->GetPreferredSize()));
}

void DisplayOverlayController::UpdateInputMappingWidgetBounds() {
  // There is no `input_mapping_widget_` if there is no active action or gio is
  // disabled.
  if (!input_mapping_widget_) {
    return;
  }

  UpdateWidgetBoundsInRootWindow(input_mapping_widget_.get(),
                                 touch_injector_->content_bounds());
}

void DisplayOverlayController::UpdateEditingListWidgetBounds() {
  // There is no `editing_list_widget_` in view mode.
  if (!editing_list_widget_) {
    return;
  }
  if (!editing_list_origin_.has_value()) {
    editing_list_origin_ = touch_injector_->content_bounds().origin();
    editing_list_origin_.value().Offset(24, 24);
  }
  auto* list_view = editing_list_widget_->GetContentsView();
  if (!list_view) {
    LOG(ERROR) << "Editing list widget has no editing list view";
    return;
  }

  UpdateWidgetBoundsInRootWindow(
      editing_list_widget_.get(),
      gfx::Rect(editing_list_origin_.value(), list_view->GetPreferredSize()));
}

void DisplayOverlayController::UpdateEditingListWidgetPosition(
    const gfx::Vector2d& reposition_delta) {
  if (!editing_list_widget_ || !editing_list_origin_.has_value()) {
    return;
  }
  auto* list_view = editing_list_widget_->GetContentsView();
  if (!list_view) {
    LOG(ERROR) << "Editing list widget has no editing list view";
    return;
  }

  editing_list_origin_ = editing_list_origin_.value() + reposition_delta;
  SetMagneticPosition();
  UpdateWidgetBoundsInRootWindow(
      editing_list_widget_.get(),
      gfx::Rect(editing_list_origin_.value(), list_view->GetPreferredSize()));
}

void DisplayOverlayController::SetMagneticPosition() {
  if (!editing_list_origin_.has_value()) {
    return;
  }

  auto app_window_bounds = touch_injector_->content_bounds();
  auto list_preferred_size =
      editing_list_widget_->GetContentsView()->GetPreferredSize();
  // Editing list is partially outside the app:
  if (editing_list_origin_.value().x() < app_window_bounds.x()) {
    // Set the editing list at the top right if it's partially outside to the
    // right.
    editing_list_origin_.value().set_x(app_window_bounds.x() -
                                       list_preferred_size.width());
    editing_list_origin_.value().set_y(app_window_bounds.y());
    return;
  } else if (editing_list_origin_.value().x() + list_preferred_size.width() >
             app_window_bounds.right()) {
    // Set the editing list at the top left if it's partially outside to the
    // left.
    editing_list_origin_.value().set_x(app_window_bounds.right());
    editing_list_origin_.value().set_y(app_window_bounds.y());
    return;
  }

  auto app_window_center = app_window_bounds.CenterPoint();
  // Editing list is within the app:
  if (editing_list_origin_.value().x() + list_preferred_size.width() / 2 <
      app_window_center.x()) {
    // Set the editing list to the left if it's closer to the left.
    editing_list_origin_.value().set_x(app_window_bounds.x());
  } else {
    // Set the editing list to the right if it's closer to the right.
    editing_list_origin_.value().set_x(app_window_bounds.right() -
                                       list_preferred_size.width());
  }
  if (editing_list_origin_.value().y() + list_preferred_size.height() / 2 <
      app_window_center.y()) {
    // Set the editing list to the top if it's closer to the top.
    editing_list_origin_.value().set_y(app_window_bounds.y());
  } else {
    // Set the editing list to the bottom if it's closer to the bottom.
    editing_list_origin_.value().set_y(app_window_bounds.bottom() -
                                       list_preferred_size.height());
  }
}

gfx::Rect DisplayOverlayController::GetEditingListWidgetBoundsInRootWindow() {
  if (!editing_list_origin_.has_value()) {
    return gfx::Rect();
  }
  auto root_bounds =
      touch_injector_->window()->GetRootWindow()->GetBoundsInScreen();
  return root_bounds - editing_list_origin_.value().OffsetFromOrigin();
}

void DisplayOverlayController::UpdateWidgetBoundsInRootWindow(
    ash::GameDashboardWidget* widget,
    const gfx::Rect& bounds_in_root_window) {
  auto root_bounds =
      touch_injector_->window()->GetRootWindow()->GetBoundsInScreen();
  auto bounds_in_screen = bounds_in_root_window;
  bounds_in_screen.Offset(root_bounds.OffsetFromOrigin());
  widget->SetBounds(bounds_in_screen);
}

void DisplayOverlayController::OnMouseEvent(ui::MouseEvent* event) {
  if ((display_mode_ == DisplayMode::kView && !nudge_view_) ||
      event->type() != ui::ET_MOUSE_PRESSED) {
    return;
  }

  ProcessPressedEvent(*event);
}

void DisplayOverlayController::OnTouchEvent(ui::TouchEvent* event) {
  if ((display_mode_ == DisplayMode::kView && !nudge_view_) ||
      event->type() != ui::ET_TOUCH_PRESSED) {
    return;
  }
  ProcessPressedEvent(*event);
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
      bool is_enabled = ash::game_dashboard_utils::IsFlagSet(
          flags, ash::ArcGameControlsFlag::kEnabled);
      SetTouchInjectorEnable(is_enabled);

      SetInputMappingVisible(ash::game_dashboard_utils::IsFlagSet(
          flags, ash::ArcGameControlsFlag::kHint));

      bool is_edit_mode = ash::game_dashboard_utils::IsFlagSet(
          touch_injector_->window()->GetProperty(ash::kArcGameControlsFlagsKey),
          ash::ArcGameControlsFlag::kEdit);
      SetDisplayMode(is_edit_mode ? DisplayMode::kEdit : DisplayMode::kView);

      bool is_showing_menu = ash::game_dashboard_utils::IsFlagSet(
          flags, ash::ArcGameControlsFlag::kMenu);
      // Save the menu states upon menu closing.
      if (ash::game_dashboard_utils::IsFlagChanged(
              flags, old_flags, ash::ArcGameControlsFlag::kMenu) &&
          !is_showing_menu) {
        touch_injector_->OnInputMenuViewRemoved();
      }

      UpdateEventRewriteCapability();
    }
  }
}

bool DisplayOverlayController::HasMenuView() const {
  return input_menu_view_ != nullptr;
}

void DisplayOverlayController::SetInputMappingVisible(bool visible) {
  if (IsBeta()) {
    // There is no `input_mapping_widget_` if there is no active action or gio
    // is disabled.
    if (!input_mapping_widget_) {
      return;
    }
    if (visible) {
      input_mapping_widget_->Show();
    } else {
      input_mapping_widget_->Hide();
    }
  } else {
    if (!input_mapping_view_) {
      return;
    }
    input_mapping_view_->SetVisible(visible);
  }

  DCHECK(touch_injector_);
  touch_injector_->store_input_mapping_visible(visible);
}

void DisplayOverlayController::SetInputMappingVisibleTemporary() {
  if (!input_mapping_view_) {
    return;
  }
  input_mapping_view_->SetVisible(true);
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
  if (!message_ && !input_menu_view_ && !nudge_view_) {
    return;
  }

  auto root_location = event.root_location();
  // Convert the LocatedEvent root location to screen location.
  auto origin =
      touch_injector_->window()->GetRootWindow()->GetBoundsInScreen().origin();
  root_location.Offset(origin.x(), origin.y());

  if (message_) {
    auto bounds = message_->GetBoundsInScreen();
    if (!bounds.Contains(root_location)) {
      RemoveEditMessage();
    }
  }

  if (input_menu_view_) {
    auto bounds = input_menu_view_->GetBoundsInScreen();
    if (!bounds.Contains(root_location)) {
      SetDisplayModeAlpha(DisplayMode::kView);
    }
  }

  // Dismiss the nudge, regardless where the click was.
  if (nudge_view_) {
    OnNudgeDismissed();
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
  const auto& children = host_window->children();
  if (children.size() > 0u) {
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

    // Remove the floating window attached the ActionView.
    RemoveButtonLabelListWidget();
    RemoveButtonOptionsMenuWidget();
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
  RemoveButtonOptionsMenuWidget();
  RemoveEditingListWidget();
  RemoveInputMappingWidget();
}

void DisplayOverlayController::AddInputMappingWidget() {
  if (input_mapping_widget_) {
    return;
  }

  input_mapping_widget_ = CreateTransientWidget(
      touch_injector_->window(), /*widget_name=*/kInputMapping,
      /*accept_events=*/false, /*is_floating=*/false);
  input_mapping_widget_->SetContentsView(
      std::make_unique<InputMappingView>(this));
  auto* window = input_mapping_widget_->GetNativeWindow();
  window->parent()->StackChildAtBottom(window);

  UpdateInputMappingWidgetBounds();
  if (touch_injector_->input_mapping_visible()) {
    input_mapping_widget_->Show();
  }
}

void DisplayOverlayController::RemoveInputMappingWidget() {
  if (input_mapping_widget_) {
    input_mapping_widget_->Close();
    input_mapping_widget_.reset();
  }
}

void DisplayOverlayController::AddEditingListWidget() {
  if (editing_list_widget_) {
    return;
  }
  editing_list_widget_ = CreateTransientWidget(
      touch_injector_->window(), /*widget_name=*/kEditingList,
      /*accept_events=*/true, /*is_floating=*/true);
  editing_list_widget_->SetContentsView(std::make_unique<EditingList>(this));
  auto* window = editing_list_widget_->GetNativeWindow();
  window->parent()->StackChildAtTop(window);

  UpdateEditingListWidgetBounds();
  editing_list_widget_->Show();
}

void DisplayOverlayController::RemoveEditingListWidget() {
  if (editing_list_widget_) {
    editing_list_widget_->Close();
    editing_list_widget_.reset();

    TurnFlag(ash::ArcGameControlsFlag::kEdit, /*turn_on=*/false);
    UpdateEventRewriteCapability();
  }
}

void DisplayOverlayController::UpdateEventRewriteCapability() {
  ash::ArcGameControlsFlag flags =
      touch_injector_->window()->GetProperty(ash::kArcGameControlsFlagsKey);

  touch_injector_->set_can_rewrite_event(
      ash::game_dashboard_utils::IsFlagSet(
          flags, ash::ArcGameControlsFlag::kEnabled) &&
      !ash::game_dashboard_utils::IsFlagSet(flags,
                                            ash::ArcGameControlsFlag::kMenu) &&
      !ash::game_dashboard_utils::IsFlagSet(
          touch_injector_->window()->GetProperty(ash::kArcGameControlsFlagsKey),
          ash::ArcGameControlsFlag::kEdit));
}

void DisplayOverlayController::DismissEducationalViewForTesting() {
  OnEducationalViewDismissed();
}

}  // namespace arc::input_overlay
