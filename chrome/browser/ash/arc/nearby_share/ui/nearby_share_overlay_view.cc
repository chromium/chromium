// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/ui/nearby_share_overlay_view.h"

#include "base/functional/bind.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/shell_surface_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"

namespace arc {

NearbyShareOverlayView::~NearbyShareOverlayView() = default;

void NearbyShareOverlayView::Show(aura::Window* base_window,
                                  views::View* child_view) {
  auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(base_window);
  DCHECK(shell_surface_base);

  CloseOverlayOn(base_window);

  auto dialog = base::WrapUnique(new NearbyShareOverlayView(child_view));

  exo::ShellSurfaceBase::OverlayParams params(std::move(dialog));
  params.translucent = true;
  params.overlaps_frame = false;
  shell_surface_base->AddOverlay(std::move(params));
}

void NearbyShareOverlayView::CloseOverlayOn(aura::Window* base_window) {
  auto* shell_surface_base = exo::GetShellSurfaceBaseForWindow(base_window);
  if (shell_surface_base && shell_surface_base->HasOverlay())
    shell_surface_base->RemoveOverlay();
}

void NearbyShareOverlayView::AddedToWidget() {
  if (has_child_view_)
    return;

  auto& view_ax = GetWidget()->GetRootView()->GetViewAccessibility();
  view_ax.SetIsIgnored(true);
}

NearbyShareOverlayView::NearbyShareOverlayView(views::View* child_view)
    : has_child_view_(child_view) {
  if (has_child_view_) {
    SetInteriorMargin(gfx::Insets::VH(0, 32));
    SetMainAxisAlignment(views::LayoutAlignment::kCenter);
    SetCrossAxisAlignment(views::LayoutAlignment::kCenter);

    child_view->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero));

    AddChildView(child_view);
  }
}

BEGIN_METADATA(NearbyShareOverlayView)
END_METADATA

}  // namespace arc
