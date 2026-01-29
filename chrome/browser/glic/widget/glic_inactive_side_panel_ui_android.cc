// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui_android.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

// static
std::unique_ptr<GlicInactiveSidePanelUi>
GlicInactiveSidePanelUi::CreateForVisibleTab(
    base::WeakPtr<tabs::TabInterface> tab,
    GlicUiEmbedder::Delegate& delegate) {
  return base::WrapUnique(new GlicInactiveSidePanelUi(tab, delegate));
}

// static
std::unique_ptr<GlicInactiveSidePanelUi>
GlicInactiveSidePanelUi::CreateForBackgroundTab(
    base::WeakPtr<tabs::TabInterface> tab,
    GlicUiEmbedder::Delegate& delegate) {
  auto inactive_side_panel =
      base::WrapUnique(new GlicInactiveSidePanelUi(tab, delegate));
  // Mark the side panel for showing next time the tab becomes active.
  inactive_side_panel->Show(ShowOptions::ForSidePanel(*tab));
  return inactive_side_panel;
}

GlicInactiveSidePanelUi::GlicInactiveSidePanelUi(
    base::WeakPtr<tabs::TabInterface> tab,
    GlicUiEmbedder::Delegate& delegate)
    : tab_(tab), delegate_(delegate) {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (glic_side_panel_coordinator) {
    // NEEDS_ANDROID_IMPL: This needs an equivalent of the inactive view
    // controller that shows a screenshot of the web client with a scrim.
    glic_side_panel_coordinator->SetWebContents(nullptr);
  }
}

GlicInactiveSidePanelUi::~GlicInactiveSidePanelUi() = default;

Host::EmbedderDelegate* GlicInactiveSidePanelUi::GetHostEmbedderDelegate() {
  return nullptr;
}

void GlicInactiveSidePanelUi::Show(const ShowOptions& options) {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return;
  }
  bool suppress_animations = false;
  if (const auto* side_panel_options =
          std::get_if<SidePanelShowOptions>(&options.embedder_options)) {
    suppress_animations = side_panel_options->suppress_opening_animation;
  }
  glic_side_panel_coordinator->Show(suppress_animations);
}

bool GlicInactiveSidePanelUi::IsShowing() const {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return false;
  }
  return glic_side_panel_coordinator->state() !=
         GlicSidePanelCoordinator::State::kClosed;
}

void GlicInactiveSidePanelUi::Close(const CloseOptions& options) {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return;
  }
  glic_side_panel_coordinator->Close(options);
}

void GlicInactiveSidePanelUi::Focus() {}

bool GlicInactiveSidePanelUi::HasFocus() {
  return IsShowing();
}

GlicSidePanelCoordinator* GlicInactiveSidePanelUi::GetGlicSidePanelCoordinator()
    const {
  return GlicSidePanelCoordinator::GetForTab(tab_.get());
}

std::unique_ptr<GlicUiEmbedder>
GlicInactiveSidePanelUi::CreateInactiveEmbedder() const {
  return nullptr;
}

mojom::PanelState GlicInactiveSidePanelUi::GetPanelState() const {
  mojom::PanelState state;
  state.kind = glic::mojom::PanelStateKind::kHidden;
  return state;
}

gfx::Size GlicInactiveSidePanelUi::GetPanelSize() {
  return gfx::Size();
}

std::string GlicInactiveSidePanelUi::DescribeForTesting() {
  return "";
}

}  // namespace glic
