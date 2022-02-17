// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"

#include <memory>

#include "ash/frame/non_client_frame_view_ash.h"
#include "base/bind.h"
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
constexpr SkColor kEditModeBgColor = SkColorSetA(SK_ColorGRAY, 0x99);
constexpr SkColor kMenuEntryBgColor = SkColorSetA(SK_ColorWHITE, 0x99);
constexpr int kCornerRadius = 8;

}  // namespace

// TODO(djacobo): Evaluate to move this to its own class for readability.
class DisplayOverlayController::InputMappingView : public views::View {
 public:
  explicit InputMappingView(DisplayOverlayController* owner) : owner_(owner) {
    auto content_bounds = input_overlay::CalculateWindowContentBounds(
        owner_->touch_injector_->target_window());
    auto& actions = owner_->touch_injector_->actions();
    SetBounds(content_bounds.x(), content_bounds.y(), content_bounds.width(),
              content_bounds.height());
    for (auto& action : actions) {
      auto view = action->CreateView(content_bounds);
      if (view)
        AddChildView(std::move(view));
    }
  }
  InputMappingView(const InputMappingView&) = delete;
  InputMappingView& operator=(const InputMappingView&) = delete;
  ~InputMappingView() override = default;

  void SetDisplayMode(const DisplayMode mode) {
    if (current_display_mode_ == mode)
      return;
    switch (mode) {
      case DisplayMode::kMenu:
      case DisplayMode::kView:
        SetBackground(nullptr);
        break;
      case DisplayMode::kEdit:
        SetBackground(views::CreateSolidBackground(kEditModeBgColor));
        break;
      default:
        NOTREACHED();
        break;
    }
    for (auto* view : children()) {
      auto* action_view = static_cast<ActionView*>(view);
      action_view->SetDisplayMode(mode);
    }
    current_display_mode_ = mode;
  }

 private:
  DisplayOverlayController* const owner_;
  DisplayMode current_display_mode_ = DisplayMode::kNone;
};

DisplayOverlayController::DisplayOverlayController(
    TouchInjector* touch_injector)
    : touch_injector_(touch_injector) {
  AddOverlay();
  touch_injector_->set_display_overlay_controller(this);
  // TODO(cuicuiruan): Initially it should be in |kEducation| mode when
  // launching and showing the educational dialog. Redo the logic here when the
  // educational dialog is ready.
  SetDisplayMode(DisplayMode::kView);
}

DisplayOverlayController::~DisplayOverlayController() {
  RemoveOverlayIfAny();
}

void DisplayOverlayController::OnWindowBoundsChanged() {
  SetDisplayMode(DisplayMode::kNone);
  SetDisplayMode(DisplayMode::kView);
}

// For test:
gfx::Rect DisplayOverlayController::GetInputMappingViewBoundsForTesting() {
  return input_mapping_view_ ? input_mapping_view_->bounds() : gfx::Rect();
}

void DisplayOverlayController::AddOverlay() {
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

  SetDisplayMode(DisplayMode::kView);
}

void DisplayOverlayController::RemoveOverlayIfAny() {
  auto* shell_surface_base =
      exo::GetShellSurfaceBaseForWindow(touch_injector_->target_window());
  if (shell_surface_base && shell_surface_base->HasOverlay())
    shell_surface_base->RemoveOverlay();
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
  input_mapping_view_->SetPosition(gfx::Point());
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
}

void DisplayOverlayController::RemoveInputMappingView() {
  if (!input_mapping_view_)
    return;
  input_mapping_view_->parent()->RemoveChildViewT(input_mapping_view_);
  input_mapping_view_ = nullptr;
}

void DisplayOverlayController::RemoveMenuEntryView() {
  if (!menu_entry_)
    return;
  menu_entry_->parent()->RemoveChildViewT(menu_entry_);
  menu_entry_ = nullptr;
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
      break;
    case DisplayMode::kEducation:
      // TODO(cuicuiruan): Add educational dialog.
      overlay_widget->GetNativeWindow()->SetEventTargetingPolicy(
          aura::EventTargetingPolicy::kTargetAndDescendants);
      break;
    case DisplayMode::kView:
      RemoveInputMenuView();
      AddInputMappingView(overlay_widget);
      AddMenuEntryView(overlay_widget);
      overlay_widget->GetNativeWindow()->SetEventTargetingPolicy(
          aura::EventTargetingPolicy::kNone);
      break;
    case DisplayMode::kEdit:
      RemoveInputMenuView();
      RemoveMenuEntryView();
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

bool DisplayOverlayController::HasMenuView() const {
  return input_menu_view_ != nullptr;
}

void DisplayOverlayController::SetInputMappingVisible(bool visible) {
  if (!input_mapping_view_)
    return;
  input_mapping_view_->SetVisible(visible);
}

bool DisplayOverlayController::GetInputMappingViewVisible() const {
  if (!input_mapping_view_)
    return false;
  return input_mapping_view_->GetVisible();
}

}  // namespace input_overlay
}  // namespace arc
