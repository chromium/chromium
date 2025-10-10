// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_side_panel_ui.h"

#include "base/notimplemented.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "components/tabs/public/tab_interface.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/view.h"

namespace glic {

GlicSidePanelUi::GlicSidePanelUi(Profile* profile,
                                 base::WeakPtr<tabs::TabInterface> tab,
                                 GlicUiEmbedder::Delegate& delegate)
    : profile_(profile), tab_(tab), delegate_(delegate) {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return;
  }

  panel_visibility_subscription_ =
      glic_side_panel_coordinator->AddStateCallback(
          base::BindRepeating(&GlicSidePanelUi::SidePanelStateChanged,
                              weak_ptr_factory_.GetWeakPtr()));

  glic_side_panel_coordinator->SetContentsView(CreateView(profile_));
  panel_state_.kind = mojom::PanelState_Kind::kAttached;
}

std::unique_ptr<views::View> GlicSidePanelUi::CreateView(Profile* profile) {
  auto glic_view = std::make_unique<GlicView>(
      profile, GlicWidget::GetInitialSize(), nullptr);
  glic_view->SetWebContents(delegate_->host().webui_contents());
  glic_view->UpdateBackgroundColor();
  return glic_view;
}

GlicSidePanelUi::~GlicSidePanelUi() = default;

Host::EmbedderDelegate* GlicSidePanelUi::GetHostEmbedderDelegate() {
  return this;
}

mojom::PanelState GlicSidePanelUi::GetPanelState() const {
  return panel_state_;
}

gfx::Size GlicSidePanelUi::GetPanelSize() {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator || !glic_side_panel_coordinator->GetView()) {
    return {};
  }

  return glic_side_panel_coordinator->GetView()->size();
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
  // The Side Panel Ui is already attached.
  NOTREACHED();
}

void GlicSidePanelUi::Detach() {
  if (!tab_) {
    return;
  }
  delegate_->Detach(tab_.get());
}

void GlicSidePanelUi::SetMinimumWidgetSize(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

bool GlicSidePanelUi::IsShowing() const {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return false;
  }
  return glic_side_panel_coordinator->IsShowing();
}

void GlicSidePanelUi::Focus() {
  auto* web_contents = delegate_->host().webui_contents();
  if (web_contents) {
    web_contents->Focus();
  }
}

void GlicSidePanelUi::SidePanelStateChanged(
    GlicSidePanelCoordinator::State state) {
  // Showing only happens through glic entrypoint, hiding can also be triggered
  // by side panel coordinator when replacing glic with another entry.
  if (state != GlicSidePanelCoordinator::State::kShown && tab_) {
    delegate_->WillCloseFor(tab_.get());
  }
}

void GlicSidePanelUi::SwitchConversation(
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::SwitchConversationCallback callback) {
  delegate_->SwitchConversation(SidePanelShowOptions(*tab_), std::move(info),
                                std::move(callback));
}

void GlicSidePanelUi::Show() {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return;
  }
  panel_state_.kind = mojom::PanelState_Kind::kAttached;
  delegate_->NotifyPanelStateChanged();
  glic_side_panel_coordinator->Show();
  Focus();
}

void GlicSidePanelUi::Close() {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator || !IsShowing()) {
    return;
  }
  panel_state_.kind = mojom::PanelState_Kind::kHidden;
  delegate_->NotifyPanelStateChanged();
  glic_side_panel_coordinator->Close();
}

void GlicSidePanelUi::ClosePanel() {
  Close();
}

std::unique_ptr<GlicUiEmbedder> GlicSidePanelUi::CreateInactiveEmbedder()
    const {
  return GlicInactiveSidePanelUi::CreateForVisibleTab(
      tab_, delegate_->host().webui_contents(), delegate_.get());
}

views::View* GlicSidePanelUi::GetView() {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  return glic_side_panel_coordinator ? glic_side_panel_coordinator->GetView()
                                     : nullptr;
}

GlicSidePanelCoordinator* GlicSidePanelUi::GetGlicSidePanelCoordinator() const {
  if (!tab_ || !tab_->GetTabFeatures()) {
    return nullptr;
  }
  return tab_->GetTabFeatures()->glic_side_panel_coordinator();
}

}  // namespace glic
