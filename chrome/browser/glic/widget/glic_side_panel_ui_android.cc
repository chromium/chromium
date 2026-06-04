// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_side_panel_ui_android.h"

#include "base/notimplemented.h"
#include "base/notreached.h"
#include "base/scoped_observation.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/glic/public/widget/glic_side_panel_coordinator_android.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/glic/widget/conversions.h"
#include "chrome/browser/glic/widget/glic_inactive_side_panel_ui_android.h"
#include "chrome/browser/ui/browser_window/public/browser_collection.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"
#include "ui/base/base_window.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_util.h"
#include "ui/snapshot/snapshot.h"

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
    delegate_->OnEmbedderWindowActivationChanged(
        browser_window->GetWindow()->IsActive());
  }

  content::WebContents* web_contents = delegate_->host().webui_contents();
  if (web_contents) {
    web_contents->SetDelegate(this);
  }

  glic_side_panel_coordinator->SetWebContents(web_contents);

  panel_state_.kind = mojom::PanelStateKind::kAttached;
}

GlicSidePanelUi::~GlicSidePanelUi() {
  content::WebContents* web_contents = delegate_->host().webui_contents();
  if (web_contents && web_contents->GetDelegate() == this) {
    web_contents->SetDelegate(nullptr);
  }
}

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

  glic_side_panel_coordinator->Show(ConvertToCoordinatorShowOptions(
      options, glic_side_panel_coordinator->SupportsPeek()));
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
  gfx::NativeWindow native_window =
      (tab_ && tab_->GetContents())
          ? tab_->GetContents()->GetTopLevelNativeWindow()
          : nullptr;

  if (!native_window) {
    std::move(callback).Run(mojom::CaptureScreenshotResult::NewErrorReason(
        mojom::CaptureScreenshotErrorReason::kUnknown));
    return;
  }

  ui::GrabWindowSnapshot(
      native_window, gfx::Rect(native_window->GetPhysicalBackingSize()),
      base::BindOnce(&GlicSidePanelUi::OnScreenshotCaptured,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void GlicSidePanelUi::OnScreenshotCaptured(
    glic::mojom::WebClientHandler::CaptureScreenshotCallback callback,
    gfx::Image snapshot) {
  if (snapshot.IsEmpty()) {
    std::move(callback).Run(mojom::CaptureScreenshotResult::NewErrorReason(
        mojom::CaptureScreenshotErrorReason::kUnknown));
    return;
  }

  auto jpeg_data = gfx::JPEG1xEncodedDataFromImage(snapshot, 100);
  if (!jpeg_data || jpeg_data->empty()) {
    std::move(callback).Run(mojom::CaptureScreenshotResult::NewErrorReason(
        mojom::CaptureScreenshotErrorReason::kUnknown));
    return;
  }

  mojom::ScreenshotPtr mojo_screenshot = mojom::Screenshot::New();
  mojo_screenshot->width_pixels = snapshot.Width();
  mojo_screenshot->height_pixels = snapshot.Height();
  mojo_screenshot->mime_type = "image/jpeg";
  mojo_screenshot->data = std::move(*jpeg_data);
  mojo_screenshot->origin_annotations = mojom::ImageOriginAnnotations::New();

  std::move(callback).Run(mojom::CaptureScreenshotResult::NewScreenshot(
      std::move(mojo_screenshot)));
}

bool GlicSidePanelUi::IsShowing() const {
  return GlicSidePanelCoordinator::IsShowing(tab_.get());
}

bool GlicSidePanelUi::IsShowingOrBackgrounded() const {
  return GlicSidePanelCoordinator::IsShowingOrBackgrounded(tab_.get());
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

namespace {
EmbedderCloseReason MapStateToCloseReason(
    GlicSidePanelCoordinator::State state) {
  switch (state) {
    case GlicSidePanelCoordinator::State::kBackgrounded:
      return EmbedderCloseReason::kBackgrounded;
    case GlicSidePanelCoordinator::State::kPeek:
      return EmbedderCloseReason::kPeek;
    case GlicSidePanelCoordinator::State::kClosed:
      return EmbedderCloseReason::kExplicitlyClosed;
    case GlicSidePanelCoordinator::State::kShown:
      NOTREACHED()
          << "This mapping is only called when the state is not kShown";
  }
}
}  // namespace

void GlicSidePanelUi::SidePanelStateChanged(
    GlicSidePanelCoordinator::State state) {
  if (state != GlicSidePanelCoordinator::State::kShown && tab_) {
    GlicInstanceMetrics::CloseReason reason =
        state == GlicSidePanelCoordinator::State::kBackgrounded
            ? GlicInstanceMetrics::CloseReason::kTabSwitched
            : GlicInstanceMetrics::CloseReason::kExplicitlyClosed;
    instance_metrics_->OnSidePanelClosed(tab_.get(), reason);
    panel_state_.kind = mojom::PanelStateKind::kHidden;
    delegate_->NotifyPanelStateChanged();

    // NOTE: `this` will be destroyed after this call.
    delegate_->DidCloseFor(tab_.get(), MapStateToCloseReason(state));
  }
}

GlicSidePanelCoordinator* GlicSidePanelUi::GetGlicSidePanelCoordinator() const {
  return GlicSidePanelCoordinator::GetForTab(tab_.get());
}

void GlicSidePanelUi::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const blink::mojom::FileChooserParams& params) {
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

}  // namespace glic
