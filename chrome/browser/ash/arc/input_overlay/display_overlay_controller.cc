// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"

#include "ash/frame/non_client_frame_view_ash.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"

namespace arc {

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
  AddInputMappingView();
}

DisplayOverlayController::~DisplayOverlayController() {
  RemoveOverlayIfAny();
}

void DisplayOverlayController::OnWindowBoundsChanged() {
  RemoveInputMappingView();
  AddInputMappingView();
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

void DisplayOverlayController::AddInputMappingView() {
  if (input_mapping_view_)
    return;
  auto* overlay_widget = GetOverlayWidget();
  DCHECK(overlay_widget);
  if (!overlay_widget)
    return;
  input_mapping_view_ = overlay_widget->GetContentsView()->AddChildView(
      std::make_unique<InputMappingView>(this));
  input_mapping_view_->SetPosition(gfx::Point());
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

}  // namespace arc
