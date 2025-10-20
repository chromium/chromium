// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui.h"

#include "base/notimplemented.h"
#include "chrome/browser/glic/widget/glic_side_panel_ui.h"
#include "chrome/browser/glic/widget/inactive_view_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/side_panel/glic/glic_side_panel_coordinator.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace glic {

// static
std::unique_ptr<GlicInactiveSidePanelUi>
GlicInactiveSidePanelUi::CreateForVisibleTab(
    base::WeakPtr<tabs::TabInterface> tab,
    content::WebContents* glic_webui_contents,
    GlicUiEmbedder::Delegate& delegate) {
  // Using `new` to access a private constructor.
  auto inactive_side_panel =
      base::WrapUnique(new GlicInactiveSidePanelUi(tab, delegate));

  // Capture screenshot asynchronously and update the inactive panel.
  inactive_side_panel->inactive_view_controller_.CaptureScreenshot(
      glic_webui_contents);

  return inactive_side_panel;
}

// static
std::unique_ptr<GlicInactiveSidePanelUi>
GlicInactiveSidePanelUi::CreateForBackgroundTab(
    base::WeakPtr<tabs::TabInterface> tab,
    content::WebContents* glic_webui_contents,
    GlicUiEmbedder::Delegate& delegate) {
  // Using `new` to access a private constructor.
  auto inactive_side_panel =
      base::WrapUnique(new GlicInactiveSidePanelUi(tab, delegate));
  // Mark the side panel for showing next time the tab becomes active.
  inactive_side_panel->Show();
  inactive_side_panel->inactive_view_controller_.CaptureScreenshot(
      glic_webui_contents);
  return inactive_side_panel;
}

GlicInactiveSidePanelUi::GlicInactiveSidePanelUi(
    base::WeakPtr<tabs::TabInterface> tab,
    GlicUiEmbedder::Delegate& delegate)
    : tab_(tab), delegate_(delegate) {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return;
  }

  auto view = inactive_view_controller_.CreateView();
  scoped_view_observation_.Observe(view.get());
  glic_side_panel_coordinator->SetContentsView(std::move(view));
}

GlicInactiveSidePanelUi::~GlicInactiveSidePanelUi() = default;

// When the user clicks on the inactive panel, the FocusableView requests focus,
// which triggers this method and activates the Glic side panel for the current
// tab.
void GlicInactiveSidePanelUi::OnViewFocused(views::View* observed_view) {
  if (tab_) {
    // NOTE: `this` will be destroyed after this call.
    delegate_->Show(ShowOptions::ForSidePanel(*tab_));
  }
}

void GlicInactiveSidePanelUi::OnViewIsDeleting(views::View* observed_view) {
  scoped_view_observation_.Reset();
}

Host::EmbedderDelegate* GlicInactiveSidePanelUi::GetHostEmbedderDelegate() {
  // This should not be called for an inactive embedder. The delegate is managed
  // by the GlicInstanceImpl.
  NOTREACHED();
}

bool GlicInactiveSidePanelUi::IsShowing() const {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return false;
  }
  return glic_side_panel_coordinator->IsShowing();
}

void GlicInactiveSidePanelUi::Show() {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return;
  }
  glic_side_panel_coordinator->Show();
}

void GlicInactiveSidePanelUi::Close() {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return;
  }
  glic_side_panel_coordinator->Close();
}

base::WeakPtr<views::View> GlicInactiveSidePanelUi::GetView() {
  return nullptr;
}

void GlicInactiveSidePanelUi::Focus() {
  // Do nothing. Inactive view doesn't have webcontents to set focus on.
}

mojom::PanelState GlicInactiveSidePanelUi::GetPanelState() const {
  mojom::PanelState state;
  state.kind = glic::mojom::PanelStateKind::kHidden;
  return state;
}

gfx::Size GlicInactiveSidePanelUi::GetPanelSize() {
  return gfx::Size();
}

std::unique_ptr<GlicUiEmbedder>
GlicInactiveSidePanelUi::CreateInactiveEmbedder() const {
  NOTREACHED() << "The embedder is already inactive.";
}

GlicSidePanelCoordinator* GlicInactiveSidePanelUi::GetGlicSidePanelCoordinator()
    const {
  if (!tab_ || !tab_->GetTabFeatures()) {
    return nullptr;
  }
  return tab_->GetTabFeatures()->glic_side_panel_coordinator();
}

}  // namespace glic
