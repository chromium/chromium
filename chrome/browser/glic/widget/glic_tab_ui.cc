// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_tab_ui.h"

#include "chrome/browser/glic/host/context/glic_screenshot_capturer.h"
#include "chrome/browser/glic/host/glic_ui.h"
#include "chrome/browser/glic/widget/glic_inactive_tab_ui.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"
#include "ui/gfx/geometry/size.h"

#if !BUILDFLAG(IS_ANDROID)
#include "ui/views/widget/widget.h"
#endif

namespace glic {

GlicTabUi::GlicTabUi(base::WeakPtr<tabs::TabInterface> tab,
                     GlicUiEmbedder::Delegate& delegate)
    : tab_(tab), delegate_(delegate) {
  browser_observation_.Observe(GlobalBrowserCollection::GetInstance());
  if (auto* browser_window = tab_->GetBrowserWindowInterface()) {
    delegate_->OnEmbedderWindowActivationChanged(
        browser_window->GetWindow()->IsActive());
  }
}

GlicTabUi::~GlicTabUi() = default;

Host::EmbedderDelegate* GlicTabUi::GetHostEmbedderDelegate() {
  return this;
}

void GlicTabUi::Show(const ShowOptions& options) {
  if (!tab_) {
    return;
  }
  if (auto* browser = tab_->GetBrowserWindowInterface()) {
    if (auto* tab_list = TabListInterface::From(browser)) {
      tab_list->ActivateTab(tab_->GetHandle());
    }
  }
}

bool GlicTabUi::IsShowing() const {
  return tab_ ? tab_->IsActivated() : false;
}

bool GlicTabUi::IsShowingOrBackgrounded() const {
  return tab_ != nullptr;
}

void GlicTabUi::Close(const CloseOptions& options) {
  if (tab_) {
    tab_->Close();
  }
}

void GlicTabUi::Focus() {
  if (tab_ && tab_->GetContents()) {
    tab_->GetContents()->Focus();
  }
}

bool GlicTabUi::HasFocus() {
  if (tab_ && tab_->GetContents()) {
    return tab_->GetContents()->GetFocusedFrame() != nullptr;
  }
  return false;
}

#if !BUILDFLAG(IS_ANDROID)
base::WeakPtr<views::View> GlicTabUi::GetView() {
  return nullptr;
}
#endif

std::unique_ptr<GlicUiEmbedder> GlicTabUi::CreateInactiveEmbedder() const {
  return std::make_unique<GlicInactiveTabUi>(tab_, *delegate_);
}

mojom::PanelState GlicTabUi::GetPanelState() const {
  // TODO(crbug.com/530661117): Add a separate PanelStateKind::kTab for full tab
  // embedder once Glic Tab support is upstreamed.
  return mojom::PanelState(mojom::PanelStateKind::kAttached, std::nullopt);
}

gfx::Size GlicTabUi::GetPanelSize() {
  if (tab_ && tab_->GetContents()) {
    return tab_->GetContents()->GetContainerBounds().size();
  }
  return gfx::Size();
}

std::string GlicTabUi::DescribeForTesting() {
  return "GlicTabUi";
}

void GlicTabUi::Attach() {
  // GlicTabUi does not have a widget/view container to attach, as the host
  // WebContents is displayed directly in the browser tab.
}

void GlicTabUi::Detach() {
  // Detaching Glic from a tab to floating mode is not supported.
}

void GlicTabUi::ClosePanel() {
  Close(CloseOptions{});
}

void GlicTabUi::OnReload() {
  // TODO(b/534799180): Handle page reloads in tab mode.
}

void GlicTabUi::CaptureScreenshot(
    glic::mojom::WebClientHandler::CaptureScreenshotCallback callback) {
  if (!tab_) {
    std::move(callback).Run(nullptr);
    return;
  }
  if (!screenshot_capturer_) {
    screenshot_capturer_ = GlicScreenshotCapturer::Create();
  }
  auto* browser_window = tab_->GetBrowserWindowInterface();
  CHECK(browser_window);
  screenshot_capturer_->CaptureScreenshot(
      browser_window->GetWindow()->GetNativeWindow(), std::move(callback));
}

void GlicTabUi::SwitchConversation(
    glic::mojom::ConversationInfoPtr info,
    mojom::WebClientHandler::SwitchConversationCallback callback) {
  delegate_->SwitchConversation(ShowOptions::ForTab(*tab_), std::move(info),
                                std::move(callback));
}

void GlicTabUi::OnMicrophoneStatusChanged(mojom::MicrophoneStatus status) {
  // Microphone status changes only affect floating UI auto-dismissal.
}

void GlicTabUi::OnBrowserActivated(BrowserWindowInterface* browser) {
  if (tab_ && tab_->GetBrowserWindowInterface() == browser) {
    delegate_->OnEmbedderWindowActivationChanged(true);
  }
}

void GlicTabUi::OnBrowserDeactivated(BrowserWindowInterface* browser) {
  if (tab_ && tab_->GetBrowserWindowInterface() == browser) {
    delegate_->OnEmbedderWindowActivationChanged(false);
  }
}

}  // namespace glic
