// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/overlay_dialog.h"

#include "ash/components/arc/compat_mode/style/arc_color_provider.h"
#include "ash/style/ash_color_id.h"
#include "base/functional/bind.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/view.h"

namespace arc {

OverlayDialog::~OverlayDialog() = default;

void OverlayDialog::Show(aura::Window* base_window,
                         base::OnceClosure on_destroying,
                         std::unique_ptr<views::View> dialog_view) {
  auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(base_window);
  if (!shell_surface_base)
    return;

  CloseIfAny(base_window);

  auto dialog = base::WrapUnique(
      new OverlayDialog(std::move(on_destroying), std::move(dialog_view)));

  exo::ShellSurfaceBase::OverlayParams params(std::move(dialog));
  params.translucent = true;
  params.overlaps_frame = false;
  params.focusable = false;

  if (chromeos::features::IsRoundedWindowsEnabled()) {
    auto window_radii = shell_surface_base->window_corners_radii();
    if (window_radii) {
      // The OverlayDialog covers the content area of the arc window. To match
      // the rounded corners of the window, we need to round the bottom two
      // corners of the overlay as well.
      params.corners_radii = gfx::RoundedCornersF(
          0, 0, window_radii->lower_right(), window_radii->lower_left());
    }
  }

  shell_surface_base->AddOverlay(std::move(params));
}

void OverlayDialog::CloseIfAny(aura::Window* base_window) {
  auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(base_window);
  if (shell_surface_base && shell_surface_base->HasOverlay())
    shell_surface_base->RemoveOverlay();
}

void OverlayDialog::AddedToWidget() {
  if (has_dialog_view_)
    return;

  auto& view_ax = GetWidget()->GetRootView()->GetViewAccessibility();
  view_ax.SetIsIgnored(true);
}

void OverlayDialog::OnThemeChanged() {
  views::View::OnThemeChanged();
  SetBackground(
      views::CreateThemedSolidBackground(ash::kColorAshShieldAndBase60));
}

OverlayDialog::OverlayDialog(base::OnceClosure on_destroying,
                             std::unique_ptr<views::View> dialog_view)
    : has_dialog_view_(dialog_view),
      scoped_callback_(std::move(on_destroying)) {
  if (dialog_view) {
    SetInteriorMargin(gfx::Insets::VH(0, 32));
    SetMainAxisAlignment(views::LayoutAlignment::kCenter);
    SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

    dialog_view->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero));

    AddChildView(std::move(dialog_view));
  }
}

BEGIN_METADATA(OverlayDialog)
END_METADATA

}  // namespace arc
