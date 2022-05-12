// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"

#include <memory>

#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/shell.h"
#include "base/bind.h"
#include "chrome/browser/ash/arc/input_overlay/ui/edit_mode_exit_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/educational_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/error_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_menu_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget.h"

namespace arc {
namespace input_overlay {

namespace {
// UI specs.
constexpr int kMenuEntrySize = 56;
constexpr int kMenuEntrySideMargin = 24;
constexpr int kEditModeExitWidth = 140;
constexpr int kEditModeExitHeight = 184;
constexpr SkColor kMenuEntryBgColor = SkColorSetA(SK_ColorWHITE, 0x99);
constexpr int kCornerRadius = 8;

}  // namespace

DisplayOverlayController::DisplayOverlayController(
    TouchInjector* touch_injector,
    bool first_launch)
    : touch_injector_(touch_injector) {
  AddOverlay(first_launch ? DisplayMode::kEducation : DisplayMode::kView);
  touch_injector_->set_display_overlay_controller(this);
  ash::Shell::Get()->AddPreTargetHandler(this);
}

DisplayOverlayController::~DisplayOverlayController() {
  RemoveOverlayIfAny();
  ash::Shell::Get()->RemovePreTargetHandler(this);
}

void DisplayOverlayController::OnWindowBoundsChanged() {
  auto mode = display_mode_;
  SetDisplayMode(DisplayMode::kNone);
  // Transition to |kView| mode except while on |kEducation| mode since
  // displaying this UI needs to be ensured as the user shouldn't be able to
  // manually access said view.
  if (mode != DisplayMode::kEducation)
    mode = DisplayMode::kView;
  SetDisplayMode(mode);
}

// For test:
gfx::Rect DisplayOverlayController::GetInputMappingViewBoundsForTesting() {
  return input_mapping_view_ ? input_mapping_view_->bounds() : gfx::Rect();
}

void DisplayOverlayController::AddOverlay(DisplayMode display_mode) {
  RemoveOverlayIfAny();
  auto* shell_surface_base =
      exo::GetShellSurfaceBaseForWindow(touch_injector_->target_window());
  if (!shell_surface_base)
    return;

  auto view = std::make_unique<views::View>();
  exo::ShellSurfaceBase::OverlayParams params(std::move(view));
  params.translucent = true;
  params.overlaps_frame = false;
  params.focusable = true;
  shell_surface_base->AddOverlay(std::move(params));

  SetDisplayMode(display_mode);
}

void DisplayOverlayController::RemoveOverlayIfAny() {
  auto* shell_surface_base =
      exo::GetShellSurfaceBaseForWindow(touch_injector_->target_window());
  if (shell_surface_base && shell_surface_base->HasOverlay())
    shell_surface_base->RemoveOverlay();
}

void DisplayOverlayController::AddMenuEntryView(views::Widget* overlay_widget) {
  if (menu_entry_)
    return;
  DCHECK(overlay_widget);
  auto game_icon = gfx::CreateVectorIcon(
      vector_icons::kVideogameAssetOutlineIcon, SK_ColorBLACK);

  // Create and position entry point for |InputMenuView|.
  auto menu_entry = std::make_unique<views::ImageButton>(base::BindRepeating(
      &DisplayOverlayController::OnMenuEntryPressed, base::Unretained(this)));
  menu_entry->SetImage(views::Button::STATE_NORMAL, game_icon);
  menu_entry->SetBackground(
      views::CreateRoundedRectBackground(kMenuEntryBgColor, kCornerRadius));
  menu_entry->SetSize(gfx::Size(kMenuEntrySize, kMenuEntrySize));
  menu_entry->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  menu_entry->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  // TODO(djacobo): Set proper positioning based on specs and responding to
  // resize.
  menu_entry->SetPosition(CalculateMenuEntryPosition());
  // TODO(djacobo): come up with a new resource for this so it can be
  // translated, or just keep reusing the one I set here.
  menu_entry->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_MENU_ENTRY_BUTTON));

  auto* parent_view = overlay_widget->GetContentsView();
  DCHECK(parent_view);
  menu_entry_ = parent_view->AddChildView(std::move(menu_entry));
}

void DisplayOverlayController::RemoveMenuEntryView() {
  if (!menu_entry_)
    return;
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
      InputMenuView::BuildMenuView(this, menu_entry_));
}

void DisplayOverlayController::RemoveInputMenuView() {
  if (!input_menu_view_)
    return;
  input_menu_view_->parent()->RemoveChildViewT(input_menu_view_);
  input_menu_view_ = nullptr;
  touch_injector_->OnInputMenuViewRemoved();
}

void DisplayOverlayController::AddInputMappingView(
    views::Widget* overlay_widget) {
  if (input_mapping_view_)
    return;
  DCHECK(overlay_widget);
  auto input_mapping_view = std::make_unique<InputMappingView>(this);
  input_mapping_view->SetPosition(gfx::Point());
  input_mapping_view_ = overlay_widget->GetContentsView()->AddChildView(
      std::move(input_mapping_view));

  // Set input mapping view visibility according to the saved status.
  DCHECK(touch_injector_);
  if (touch_injector_)
    SetInputMappingVisible(touch_injector_->input_mapping_visible());
}

void DisplayOverlayController::RemoveInputMappingView() {
  if (!input_mapping_view_)
    return;
  input_mapping_view_->parent()->RemoveChildViewT(input_mapping_view_);
  input_mapping_view_ = nullptr;
}

void DisplayOverlayController::AddEditModeExitView(
    views::Widget* overlay_widget) {
  DCHECK(overlay_widget);
  auto* parent_view = overlay_widget->GetContentsView();
  DCHECK(parent_view);

  // TODO(djacobo): Undefined vertical position, reusing whatever |entry_menu_|
  // uses for now.
  edit_mode_view_ = parent_view->AddChildView(
      EditModeExitView::BuildView(this, CalculateEditModeExitPosition()));
}

void DisplayOverlayController::RemoveEditModeExitView() {
  if (!edit_mode_view_)
    return;
  edit_mode_view_->parent()->RemoveChildViewT(edit_mode_view_);
  edit_mode_view_ = nullptr;
}

void DisplayOverlayController::AddEducationalView() {
  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  auto* parent_view = overlay_widget->GetContentsView();
  DCHECK(parent_view);
  if (educational_view_)
    return;

  educational_view_ = parent_view->AddChildView(
      EducationalView::BuildMenu(this, GetParentView()));
}

void DisplayOverlayController::RemoveEducationalView() {
  if (!educational_view_)
    return;
  educational_view_->parent()->RemoveChildViewT(educational_view_);
  educational_view_ = nullptr;
}

void DisplayOverlayController::OnEducationalViewDismissed() {
  touch_injector_->set_first_launch(false);
  SetDisplayMode(DisplayMode::kView);
}

views::Widget* DisplayOverlayController::GetOverlayWidget() {
  auto* shell_surface_base =
      exo::GetShellSurfaceBaseForWindow(touch_injector_->target_window());
  DCHECK(shell_surface_base);

  return shell_surface_base ? static_cast<views::Widget*>(
                                  shell_surface_base->GetFocusTraversable())
                            : nullptr;
}

gfx::Point DisplayOverlayController::CalculateMenuEntryPosition() {
  auto* overlay_widget = GetOverlayWidget();
  if (!overlay_widget)
    return gfx::Point();
  auto* view = overlay_widget->GetContentsView();
  if (!view || view->bounds().IsEmpty())
    return gfx::Point();

  return gfx::Point(
      std::max(0, view->width() - kMenuEntrySize - kMenuEntrySideMargin),
      std::max(0, view->height() / 2 - kMenuEntrySize / 2));
}

gfx::Point DisplayOverlayController::CalculateEditModeExitPosition() {
  auto* overlay_widget = GetOverlayWidget();
  if (!overlay_widget)
    return gfx::Point();
  auto* view = overlay_widget->GetContentsView();
  if (!view || view->bounds().IsEmpty())
    return gfx::Point();

  return gfx::Point(
      std::max(0, view->width() - kEditModeExitWidth - kMenuEntrySideMargin),
      std::max(0, view->height() / 2 - kEditModeExitHeight / 2));
}

views::View* DisplayOverlayController::GetParentView() {
  auto* overlay_widget = GetOverlayWidget();
  if (!overlay_widget)
    return nullptr;
  return overlay_widget->GetContentsView();
}

void DisplayOverlayController::SetDisplayMode(DisplayMode mode) {
  if (display_mode_ == mode)
    return;

  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  if (!overlay_widget)
    return;

  switch (mode) {
    case DisplayMode::kNone:
      RemoveMenuEntryView();
      RemoveInputMappingView();
      RemoveEditModeExitView();
      break;
    case DisplayMode::kEducation:
      AddEducationalView();
      overlay_widget->GetNativeWindow()->SetEventTargetingPolicy(
          aura::EventTargetingPolicy::kTargetAndDescendants);
      break;
    case DisplayMode::kView:
      RemoveInputMenuView();
      RemoveEditModeExitView();
      RemoveEducationalView();
      AddInputMappingView(overlay_widget);
      AddMenuEntryView(overlay_widget);
      overlay_widget->GetNativeWindow()->SetEventTargetingPolicy(
          aura::EventTargetingPolicy::kNone);
      break;
    case DisplayMode::kEdit:
      RemoveInputMenuView();
      RemoveMenuEntryView();
      RemoveEducationalView();
      AddEditModeExitView(overlay_widget);
      overlay_widget->GetNativeWindow()->SetEventTargetingPolicy(
          aura::EventTargetingPolicy::kTargetAndDescendants);
      break;
    case DisplayMode::kMenu:
      overlay_widget->GetNativeWindow()->SetEventTargetingPolicy(
          aura::EventTargetingPolicy::kTargetAndDescendants);
      break;
    default:
      NOTREACHED();
      break;
  }

  if (input_mapping_view_)
    input_mapping_view_->SetDisplayMode(mode);

  DCHECK(touch_injector_);
  if (touch_injector_)
    touch_injector_->set_display_mode(mode);

  display_mode_ = mode;
}

absl::optional<gfx::Rect>
DisplayOverlayController::GetOverlayMenuEntryBounds() {
  if (!menu_entry_)
    return absl::nullopt;

  return absl::optional<gfx::Rect>(menu_entry_->bounds());
}

void DisplayOverlayController::AddActionEditMenu(ActionView* anchor,
                                                 ActionType action_type) {
  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  if (!overlay_widget)
    return;
  auto* parent_view = overlay_widget->GetContentsView();
  DCHECK(parent_view);
  if (!parent_view)
    return;
  auto action_edit_menu =
      ActionEditMenu::BuildActionEditMenu(this, anchor, action_type);
  if (action_edit_menu)
    action_edit_menu_ = parent_view->AddChildView(std::move(action_edit_menu));
}

void DisplayOverlayController::RemoveActionEditMenu() {
  if (!action_edit_menu_)
    return;
  action_edit_menu_->parent()->RemoveChildViewT(action_edit_menu_);
  action_edit_menu_ = nullptr;
}

void DisplayOverlayController::AddEditErrorMsg(ActionView* action_view,
                                               base::StringPiece error_msg) {
  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  if (!overlay_widget)
    return;
  auto* parent_view = overlay_widget->GetContentsView();
  DCHECK(parent_view);
  if (!parent_view)
    return;
  auto error = std::make_unique<ErrorView>(this, action_view, error_msg);
  error_ = parent_view->AddChildView(std::move(error));
}

void DisplayOverlayController::RemoveEditErrorMsg() {
  if (!error_)
    return;
  error_->parent()->RemoveChildViewT(error_);
  error_ = nullptr;
}

void DisplayOverlayController::OnBindingChange(
    Action* action,
    std::unique_ptr<InputElement> input_element) {
  touch_injector_->OnBindingChange(action, std::move(input_element));
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

const std::string* DisplayOverlayController::GetPackageName() const {
  return touch_injector_->GetPackageName();
}

void DisplayOverlayController::OnApplyMenuState() {
  if (display_mode_ != DisplayMode::kView)
    return;

  SetInputMappingVisible(GetTouchInjectorEnable() &&
                         GetInputMappingViewVisible());
}

void DisplayOverlayController::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::ET_MOUSE_PRESSED)
    ProcessPressedEvent(*event);
}

void DisplayOverlayController::OnTouchEvent(ui::TouchEvent* event) {
  if (event->type() == ui::ET_TOUCH_PRESSED)
    ProcessPressedEvent(*event);
}

bool DisplayOverlayController::HasMenuView() const {
  return input_menu_view_ != nullptr;
}

void DisplayOverlayController::SetInputMappingVisible(bool visible) {
  if (!input_mapping_view_)
    return;
  input_mapping_view_->SetVisible(visible);
  DCHECK(touch_injector_);
  if (!touch_injector_)
    return;
  touch_injector_->store_input_mapping_visible(visible);
}

bool DisplayOverlayController::GetInputMappingViewVisible() const {
  DCHECK(touch_injector_);
  if (!touch_injector_)
    return false;
  return touch_injector_->input_mapping_visible();
}

void DisplayOverlayController::SetTouchInjectorEnable(bool enable) {
  DCHECK(touch_injector_);
  if (!touch_injector_)
    return;
  touch_injector_->store_touch_injector_enable(enable);
}

bool DisplayOverlayController::GetTouchInjectorEnable() {
  DCHECK(touch_injector_);
  if (!touch_injector_)
    return false;
  return touch_injector_->touch_injector_enable();
}

void DisplayOverlayController::ProcessPressedEvent(
    const ui::LocatedEvent& event) {
  if (!action_edit_menu_ && !error_)
    return;

  if (action_edit_menu_) {
    auto bounds = action_edit_menu_->GetBoundsInScreen();
    auto root_location = event.root_location();
    if (!bounds.Contains(root_location))
      RemoveActionEditMenu();
  }

  if (error_) {
    auto bounds = error_->GetBoundsInScreen();
    auto root_location = event.root_location();
    if (!bounds.Contains(root_location))
      RemoveEditErrorMsg();
  }
}

void DisplayOverlayController::DismissEducationalViewForTesting() {
  OnEducationalViewDismissed();
}

}  // namespace input_overlay
}  // namespace arc
