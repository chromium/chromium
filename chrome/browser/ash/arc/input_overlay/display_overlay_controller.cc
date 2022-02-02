// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"

#include <memory>

#include "ash/frame/non_client_frame_view_ash.h"
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
constexpr int kMinAnchorSide = 40;
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

 private:
  DisplayOverlayController* const owner_;
};

DisplayOverlayController::DisplayOverlayController(
    TouchInjector* touch_injector)
    : touch_injector_(touch_injector) {
  AddOverlay();
  AddOverlayChildrenViews();
}

DisplayOverlayController::~DisplayOverlayController() {
  RemoveOverlayIfAny();
}

void DisplayOverlayController::OnWindowBoundsChanged() {
  RemoveInputMappingView();
  AddOverlayChildrenViews();
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

  exo::ShellSurfaceBase::OverlayParams params(std::make_unique<views::View>());
  params.translucent = true;
  params.overlaps_frame = false;
  shell_surface_base->AddOverlay(std::move(params));

  views::Widget* overlay_widget =
      static_cast<views::Widget*>(shell_surface_base->GetFocusTraversable());
  // TODO(cuicuiruan): split below to the view mode. For the edit mode, display
  // overlay should take the event.
  overlay_widget->GetNativeWindow()->SetEventTargetingPolicy(
      aura::EventTargetingPolicy::kNone);
}

void DisplayOverlayController::RemoveOverlayIfAny() {
  auto* shell_surface_base =
      exo::GetShellSurfaceBaseForWindow(touch_injector_->target_window());
  if (shell_surface_base && shell_surface_base->HasOverlay())
    shell_surface_base->RemoveOverlay();
}

void DisplayOverlayController::AddOverlayChildrenViews() {
  if (input_mapping_view_)
    return;
  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  if (!overlay_widget)
    return;

  AddInputMappingView(overlay_widget);
  AddInputOverlayMenuView(overlay_widget);
}

void DisplayOverlayController::AddInputMappingView(
    views::Widget* overlay_widget) {
  DCHECK(overlay_widget);

  auto input_mapping_view = std::make_unique<InputMappingView>(this);
  input_mapping_view->SetPosition(gfx::Point());

  input_mapping_view_ = overlay_widget->GetContentsView()->AddChildView(
      std::move(input_mapping_view));
  input_mapping_view_->SetPosition(gfx::Point());
}

void DisplayOverlayController::AddInputOverlayMenuView(
    views::Widget* overlay_widget) {
  DCHECK(overlay_widget);
  auto game_icon = gfx::CreateVectorIcon(
      vector_icons::kVideogameAssetOutlineIcon, SK_ColorBLACK);

  // Create and position entry point for |InputOverlayMenuView|.
  auto overlay_menu_anchor = std::make_unique<views::ImageButton>(
      base::BindRepeating(&DisplayOverlayController::OnMenuAnchorPressed,
                          base::Unretained(this)));
  overlay_menu_anchor->SetImage(views::Button::STATE_NORMAL, game_icon);
  overlay_menu_anchor->SetBackground(
      views::CreateSolidBackground(SK_ColorWHITE));
  overlay_menu_anchor->SetSize(gfx::Size(kMinAnchorSide, kMinAnchorSide));
  overlay_menu_anchor->SetImageHorizontalAlignment(
      views::ImageButton::ALIGN_CENTER);
  overlay_menu_anchor->SetImageVerticalAlignment(
      views::ImageButton::ALIGN_MIDDLE);
  // TODO(djacobo): Set proper positioning based on specs and responding to
  // resize.
  overlay_menu_anchor->SetPosition(CalculateMenuAnchorPosition());
  // TODO(djacobo): come up with a new resource for this so it can be
  // translated, or just keep reusing the one I set here.
  overlay_menu_anchor->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_INPUT_OVERLAY_MENU_ENTRY_BUTTON));

  auto* parent_view = overlay_widget->GetContentsView();
  DCHECK(parent_view);
  overlay_menu_anchor_ =
      parent_view->AddChildView(std::move(overlay_menu_anchor));
}

void DisplayOverlayController::OnMenuAnchorPressed() {
  // TODO(djacobo): Implement calling |InputOverlayMenuView|.
}

void DisplayOverlayController::RemoveInputMappingView() {
  if (!input_mapping_view_)
    return;
  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  if (!overlay_widget)
    return;
  overlay_widget->GetContentsView()->RemoveChildViewT(input_mapping_view_);
  input_mapping_view_ = nullptr;
}

views::Widget* DisplayOverlayController::GetOverlayWidget() {
  auto* shell_surface_base =
      exo::GetShellSurfaceBaseForWindow(touch_injector_->target_window());
  DCHECK(shell_surface_base);

  return shell_surface_base ? static_cast<views::Widget*>(
                                  shell_surface_base->GetFocusTraversable())
                            : nullptr;
}

gfx::Point DisplayOverlayController::CalculateMenuAnchorPosition() {
  auto* overlay_widget = GetOverlayWidget();
  if (!overlay_widget)
    return gfx::Point();
  auto* view = overlay_widget->GetContentsView();
  if (!view || view->bounds().IsEmpty())
    return gfx::Point();

  return gfx::Point(view->width() - 2 * kMinAnchorSide, view->height() / 2);
}

}  // namespace input_overlay
}  // namespace arc
