// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_side_panel_ui_android.h"

#include "base/notimplemented.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/glic/public/widget/glic_side_panel_coordinator_android.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui_android.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"

namespace glic {

GlicSidePanelUi::GlicSidePanelUi(Profile* profile,
                                 base::WeakPtr<tabs::TabInterface> tab,
                                 GlicUiEmbedder::Delegate& delegate,
                                 GlicInstanceMetrics& instance_metrics)
    : tab_(tab), delegate_(delegate), instance_metrics_(instance_metrics) {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return;
  }

  panel_visibility_subscription_ =
      glic_side_panel_coordinator->AddStateCallback(
          base::BindRepeating(&GlicSidePanelUi::SidePanelStateChanged,
                              weak_ptr_factory_.GetWeakPtr()));

  browser_observation_.Observe(GlobalBrowserCollection::GetInstance());
  if (auto* browser_window = tab_->GetBrowserWindowInterface()) {
    bool is_active = false;
    GlobalBrowserCollection::GetInstance()->ForEach(
        [&](BrowserWindowInterface* browser) {
          if (browser == browser_window) {
            is_active = true;
          }
          return false;  // Stop after first (most active)
        },
        BrowserCollection::Order::kActivation);
    delegate_->OnEmbedderWindowActivationChanged(is_active);
  }

  glic_side_panel_coordinator->SetWebContents(
      delegate_->host().webui_contents());

  panel_state_.kind = mojom::PanelStateKind::kAttached;
}

GlicSidePanelUi::~GlicSidePanelUi() = default;

void GlicSidePanelUi::OnClientReady() {
  instance_metrics_->OnClientReady(
      GlicInstanceMetrics::EmbedderType::kSidePanel);
}

Host::EmbedderDelegate* GlicSidePanelUi::GetHostEmbedderDelegate() {
  return this;
}

void GlicSidePanelUi::Show(const ShowOptions& options) {
  instance_metrics_->OnShowInSidePanel(tab_.get());
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return;
  }
  panel_state_.kind = mojom::PanelStateKind::kAttached;
  delegate_->NotifyPanelStateChanged();
  delegate_->host().FloatingPanelCanAttachChanged(false);

  bool suppress_animations = false;
  if (const auto* side_panel_options =
          std::get_if<SidePanelShowOptions>(&options.embedder_options)) {
    suppress_animations = side_panel_options->suppress_opening_animation;
  }
  glic_side_panel_coordinator->Show(suppress_animations);
}

void GlicSidePanelUi::Close(const CloseOptions& options) {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return;
  }
  // NOTE: `this` will be destroyed after this call.
  glic_side_panel_coordinator->Close(options);
}

void GlicSidePanelUi::Focus() {
  if (auto* web_contents = delegate_->host().webui_contents()) {
    web_contents->Focus();
  }
}

bool GlicSidePanelUi::HasFocus() {
  if (auto* web_contents = delegate_->host().webui_contents()) {
    if (auto* view = web_contents->GetRenderWidgetHostView()) {
      return view->HasFocus();
    }
  }
  return false;
}

mojom::PanelState GlicSidePanelUi::GetPanelState() const {
  return panel_state_;
}

gfx::Size GlicSidePanelUi::GetPanelSize() {
  if (auto* web_contents = delegate_->host().webui_contents()) {
    return web_contents->GetContainerBounds().size();
  }
  return gfx::Size();
}

std::string GlicSidePanelUi::DescribeForTesting() {
  return base::StrCat({"SidePanelUi for tab ",
                       base::NumberToString(tab_->GetHandle().raw_value())});
}

std::unique_ptr<GlicUiEmbedder> GlicSidePanelUi::CreateInactiveEmbedder()
    const {
  return GlicInactiveSidePanelUi::CreateForVisibleTab(tab_, *delegate_);
}

void GlicSidePanelUi::Resize(const gfx::Size& size,
                             base::TimeDelta duration,
                             base::OnceClosure callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run();
}

void GlicSidePanelUi::EnableDragResize(bool enabled) {
  NOTIMPLEMENTED();
}

void GlicSidePanelUi::Attach() {
  // The Side Panel Ui is already attached, do nothing.
}

void GlicSidePanelUi::Detach() {
  if (!tab_) {
    return;
  }
  // NOTE: `this` will be destroyed after this call.
  delegate_->Detach(*tab_);
}

void GlicSidePanelUi::SetMinimumWidgetSize(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

void GlicSidePanelUi::SwitchConversation(
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::SwitchConversationCallback callback) {
  // NOTE: `this` may be destroyed after this call.
  delegate_->SwitchConversation(ShowOptions::ForSidePanel(*tab_),
                                std::move(info), std::move(callback));
}

void GlicSidePanelUi::CaptureScreenshot(
    glic::mojom::WebClientHandler::CaptureScreenshotCallback callback) {
  // Not implemented on Android yet.
  std::move(callback).Run(nullptr);
}

bool GlicSidePanelUi::IsShowing() const {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (!glic_side_panel_coordinator) {
    return false;
  }
  return glic_side_panel_coordinator->state() !=
         GlicSidePanelCoordinator::State::kClosed;
}

void GlicSidePanelUi::ClosePanel() {
  Close(CloseOptions());
}

void GlicSidePanelUi::OnReload() {
  auto* glic_side_panel_coordinator = GetGlicSidePanelCoordinator();
  if (glic_side_panel_coordinator) {
    glic_side_panel_coordinator->SetWebContents(
        delegate_->host().webui_contents());
  }
}

void GlicSidePanelUi::OnBrowserActivated(BrowserWindowInterface* browser) {
  if (tab_ && tab_->GetBrowserWindowInterface() == browser) {
    delegate_->OnEmbedderWindowActivationChanged(true);
  }
}

void GlicSidePanelUi::OnBrowserDeactivated(BrowserWindowInterface* browser) {
  if (tab_ && tab_->GetBrowserWindowInterface() == browser) {
    delegate_->OnEmbedderWindowActivationChanged(false);
  }
}

void GlicSidePanelUi::SidePanelStateChanged(
    GlicSidePanelCoordinator::State state) {
  if (state != GlicSidePanelCoordinator::State::kShown && tab_) {
    instance_metrics_->OnSidePanelClosed(tab_.get());
    panel_state_.kind = mojom::PanelStateKind::kHidden;
    delegate_->NotifyPanelStateChanged();
    // NOTE: `this` will be destroyed after this call.
    delegate_->WillCloseFor(tab_.get());
  }
}

GlicSidePanelCoordinator* GlicSidePanelUi::GetGlicSidePanelCoordinator() const {
  return GlicSidePanelCoordinator::GetForTab(tab_.get());
}

}  // namespace glic
