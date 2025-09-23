// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_side_panel_ui.h"

#include "base/notimplemented.h"
#include "chrome/browser/glic/host/glic_ui_embedder.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

GlicSidePanelUi::GlicSidePanelUi(Profile* profile,
                                 base::WeakPtr<tabs::TabInterface> tab,
                                 GlicUiEmbedder::Delegate& delegate)
    : profile_(profile), tab_(tab), delegate_(delegate) {
  if (tab_) {
    coordinator_observation_.Observe(
        tab_->GetTabFeatures()->glic_side_panel_coordinator());
  }
}

GlicSidePanelUi::~GlicSidePanelUi() = default;

Host::Delegate* GlicSidePanelUi::GetHostDelegate() {
  return this;
}

const mojom::PanelState& GlicSidePanelUi::GetPanelState() const {
  return panel_state_;
}

void GlicSidePanelUi::Resize(const gfx::Size& size,
                             base::TimeDelta duration,
                             base::OnceClosure callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run();
}

void GlicSidePanelUi::SetDraggableAreas(
    const std::vector<gfx::Rect>& draggable_areas) {
  NOTIMPLEMENTED();
}

void GlicSidePanelUi::EnableDragResize(bool enabled) {
  NOTIMPLEMENTED();
}

void GlicSidePanelUi::Attach() {
  NOTIMPLEMENTED();
}

void GlicSidePanelUi::Detach() {
  NOTIMPLEMENTED();
}

void GlicSidePanelUi::SetMinimumWidgetSize(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

bool GlicSidePanelUi::IsShowing() const {
  if (!tab_) {
    return false;
  }
  return panel_state_.kind == mojom::PanelState_Kind::kAttached;
}

void GlicSidePanelUi::SwitchConversation(
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::SwitchConversationCallback callback) {
  delegate_->SwitchConversation(tab_.get(), std::move(info),
                                std::move(callback));
}

// TODO(crbug.com/444293841): Support closing multi instance.
void GlicSidePanelUi::VisibilityChanged(bool visible) {
  // Showing only happens through glic entrypoint, hiding can also be triggered
  // by side panel coordinator when replacing glic with another entry.
  if (!visible) {
    panel_state_.kind = mojom::PanelState_Kind::kHidden;
    // TODO(crbug.com/444293841): Support closing multi instance.
  }
}

void GlicSidePanelUi::Show() {
  if (!tab_) {
    return;
  }
  panel_state_.kind = mojom::PanelState_Kind::kAttached;
  auto* side_panel_coordinator =
      tab_->GetBrowserWindowInterface()->GetFeatures().side_panel_coordinator();
  side_panel_coordinator->Show(SidePanelEntry::Id::kGlic);
}
void GlicSidePanelUi::Close() {
  if (!tab_ || !IsShowing()) {
    return;
  }
  panel_state_.kind = mojom::PanelState_Kind::kHidden;
  auto* side_panel_coordinator =
      tab_->GetBrowserWindowInterface()->GetFeatures().side_panel_coordinator();
  side_panel_coordinator->Close();
}

std::unique_ptr<views::View> GlicSidePanelUi::CreateView() {
  auto glic_view = std::make_unique<GlicView>(
      profile_, GlicWidget::GetInitialSize(), nullptr);
  // TODO(refactor): use the right host when we have multiple hosts
  glic_view->SetWebContents(delegate_->host().webui_contents());
  glic_view->UpdateBackgroundColor();
  return glic_view;
}

std::unique_ptr<GlicUiEmbedder> GlicSidePanelUi::CreateInactiveEmbedder()
    const {
  return GlicInactiveSidePanelUi::From(*this);
}

}  // namespace glic
