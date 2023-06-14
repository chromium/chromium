// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"

#include <memory>

#include "ash/components/arc/compat_mode/style/arc_color_provider.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/style/pill_button.h"
#include "ash/style/style_util.h"
#include "ash/wm/window_state.h"
#include "base/functional/bind.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
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
#include "chrome/grit/generated_resources.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_styles.h"
#include "ui/color/color_id.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace arc::input_overlay {

namespace {
// UI specs.
constexpr int kMenuEntrySideMargin = 24;
constexpr int kNudgeVerticalAlign = 8;

}  // namespace

DisplayOverlayController::DisplayOverlayController(
    TouchInjector* touch_injector,
    bool first_launch)
    : touch_injector_(touch_injector) {
  touch_injector_->set_display_overlay_controller(this);

  // There is no instance for unittest.
  if (!ash::Shell::HasInstance()) {
    return;
  }

  AddOverlay(first_launch ? DisplayMode::kEducation : DisplayMode::kView);
  ash::Shell::Get()->AddPreTargetHandler(this);
}

DisplayOverlayController::~DisplayOverlayController() {
  touch_injector_->set_display_overlay_controller(nullptr);

  // There is no instance for unittest.
  if (!ash::Shell::HasInstance()) {
    return;
  }

  ash::Shell::Get()->RemovePreTargetHandler(this);
  RemoveOverlayIfAny();
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

  SetDisplayMode(display_mode);
}

void DisplayOverlayController::RemoveOverlayIfAny() {
  if (display_mode_ == DisplayMode::kEdit) {
    OnCustomizeCancel();
  }
  auto* shell_surface_base =
      exo::GetShellSurfaceBaseForWindow(touch_injector_->window());
  if (shell_surface_base && shell_surface_base->HasOverlay()) {
    // Call |RemoveInputMenuView| explicitly to make sure UMA stats is updated.
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

void DisplayOverlayController::AddButtonOptionsMenu(Action* action) {
  if (!IsBeta() ||
      (button_options_menu_ && button_options_menu_->action() == action)) {
    return;
  }
  RemoveButtonOptionsMenu();
  button_options_menu_ = ButtonOptionsMenu::Show(this, action);
}

void DisplayOverlayController::RemoveButtonOptionsMenu() {
  if (!IsBeta() || !button_options_menu_) {
    return;
  }
  button_options_menu_->parent()->RemoveChildViewT(button_options_menu_);
  button_options_menu_ = nullptr;
}

void DisplayOverlayController::AddEditingList() {
  if (!IsBeta() || editing_list_) {
    return;
  }
  editing_list_ = EditingList::Show(this);
}

void DisplayOverlayController::RemoveEditingList() {
  if (!IsBeta() || !editing_list_) {
    return;
  }
  GetOverlayWidgetContentsView()->RemoveChildViewT(editing_list_);
  editing_list_ = nullptr;
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
  // Create and position entry point for |InputMenuView|.
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

  SetDisplayMode(DisplayMode::kMenu);

  input_menu_view_ = parent_view->AddChildView(
      InputMenuView::BuildMenuView(this, menu_entry_, parent_view->size()));
  // Hide the menu entry when the menu is displayed.
  menu_entry_->SetVisible(false);
}

void DisplayOverlayController::OnMenuEntryPositionChanged(
    bool leave_focus,
    absl::optional<gfx::Point> location) {
  if (leave_focus) {
    SetDisplayMode(DisplayMode::kView);
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
  if (touch_injector_) {
    SetInputMappingVisible(touch_injector_->input_mapping_visible());
  }
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
  SetDisplayMode(DisplayMode::kView);
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

void DisplayOverlayController::SetDisplayMode(DisplayMode mode) {
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
      RemoveButtonOptionsMenu();
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
      RemoveEditingList();
      RemoveEditFinishView();
      RemoveEducationalView();
      RemoveNudgeView();
      RemoveButtonOptionsMenu();
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
      AddEditingList();
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

absl::optional<gfx::Rect>
DisplayOverlayController::GetOverlayMenuEntryBounds() {
  if (!menu_entry_ || !menu_entry_->GetVisible()) {
    return absl::nullopt;
  }

  return absl::optional<gfx::Rect>(menu_entry_->GetBoundsInScreen());
}

void DisplayOverlayController::AddEditMessage(const base::StringPiece& message,
                                              MessageType message_type) {
  // There is no instance for unittest.
  if (!ash::Shell::HasInstance()) {
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

void DisplayOverlayController::OnCustomizeSave() {
  touch_injector_->OnBindingSave();
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

int DisplayOverlayController::GetTouchInjectorActionsSize() {
  return touch_injector_->actions().size();
}

void DisplayOverlayController::AddTouchInjectorObserver(
    TouchInjectorObserver* observer) {
  touch_injector_->AddObserver(observer);
}

void DisplayOverlayController::RemoveTouchInjectorObserver(
    TouchInjectorObserver* observer) {
  touch_injector_->RemoveObserver(observer);
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
  if (key != chromeos::kImmersiveIsActive) {
    return;
  }
  bool is_immersive = window->GetProperty(chromeos::kImmersiveIsActive);
  // This is to catch the corner case that when an app is launched as
  // fullscreen/immersive mode, so it only cares when the window turns into
  // immersive mode from non-immersive mode.
  if (!is_immersive || is_immersive == static_cast<bool>(old)) {
    return;
  }

  UpdateForBoundsChanged();
}

bool DisplayOverlayController::HasMenuView() const {
  return input_menu_view_ != nullptr;
}

void DisplayOverlayController::SetInputMappingVisible(bool visible) {
  if (!input_mapping_view_) {
    return;
  }
  input_mapping_view_->SetVisible(visible);
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
      SetDisplayMode(DisplayMode::kView);
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
  if (content_bounds == touch_injector_->content_bounds()) {
    return;
  }
  touch_injector_->UpdateForOverlayBoundsChanged(content_bounds);

  // Overlay widget is null for test.
  if (!GetOverlayWidget()) {
    return;
  }

  auto mode = display_mode_;
  SetDisplayMode(DisplayMode::kNone);
  // Transition to |kView| mode except while on |kEducation| mode since the
  // educational banner needs to remain visible until dismissed by the user.
  if (mode != DisplayMode::kEducation) {
    mode = DisplayMode::kView;
  }

  SetDisplayMode(mode);
}

void DisplayOverlayController::DismissEducationalViewForTesting() {
  OnEducationalViewDismissed();
}

}  // namespace arc::input_overlay
