// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/preview/preview_tab.h"

#include "base/features.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

class PreviewTab::WebContentsObserver final
    : public content::WebContentsObserver {
 public:
  explicit WebContentsObserver(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents) {
    UpdateZoomSettings();
  }

 private:
  // content::WebContentsObserver.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    // TODO(b:291842891): We will update zoom settings also at the preview
    // navigation.
    if (!navigation_handle->IsInPrimaryMainFrame() ||
        !navigation_handle->HasCommitted()) {
      return;
    }
    // Zoom settings will be reset by ZoomController::DidFinishNavigation when
    // the primary main frame navigation happens. We need to override them
    // again whenever the settings are reset.
    UpdateZoomSettings();
  }

  void UpdateZoomSettings() {
    auto* zoom_controller =
        zoom::ZoomController::FromWebContents(web_contents());
    zoom_controller->SetZoomMode(
        zoom::ZoomController::ZoomMode::ZOOM_MODE_ISOLATED);
    const double level = blink::PageZoomFactorToZoomLevel(0.5f);
    zoom_controller->SetZoomLevel(level);
  }
};

PreviewTab::PreviewTab(content::WebContents& parent, const GURL& url)
    : widget_(std::make_unique<views::Widget>()),
      view_(std::make_unique<views::WebView>(parent.GetBrowserContext())),
      url_(url) {
  CHECK(base::FeatureList::IsEnabled(blink::features::kLinkPreview));

  // WebView setup.
  view_->GetWebContents()->SetDelegate(this);
  AttachTabHelpersForInit();
  // Our observer should be created after ZoomController is created in
  // AttachTabHelpersForInit() above to ensure our DidFinishNavigation is called
  // after ZoomController::DidFinishNavigation resets zoom settings. This is
  // because the observer invocation order depends on its registration order.
  observer_ = std::make_unique<WebContentsObserver>(view_->GetWebContents());

  // TODO(b:292184832): Ensure if we provide enough information to perform an
  // equivalent navigation with a link navigation.
  view_->LoadInitialURL(url_);

  InitWindow(parent);
}

PreviewTab::~PreviewTab() = default;

base::WeakPtr<content::WebContents> PreviewTab::GetWebContents() {
  return view_->GetWebContents()->GetWeakPtr();
}

void PreviewTab::AttachTabHelpersForInit() {
  content::WebContents* web_contents = view_->GetWebContents();

  // TODO(b:291867757): Audit TabHelpers and determine when
  // (initiation/promotion) we should attach each of them.
  zoom::ZoomController::CreateForWebContents(web_contents);
  SecurityStateTabHelper::CreateForWebContents(web_contents);
  chrome::InitializePageLoadMetricsForWebContents(web_contents);
}

void PreviewTab::InitWindow(content::WebContents& parent) {
  // All details here are tentative until we fix the details of UI.
  //
  // TODO(go/launch/4269184): Revisit it later.

  views::Widget::InitParams params;
  // TODO(b:292184832): Create with own buttons
  params.type = views::Widget::InitParams::TYPE_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  const gfx::Rect rect = parent.GetContainerBounds();
  params.bounds =
      gfx::Rect(rect.x() + rect.width() / 2, rect.y() + rect.height() / 2,
                rect.width() / 2, rect.height() / 2);
  widget_->Init(std::move(params));
  // TODO(b:292184832): Clarify the ownership.
  widget_->non_client_view()->frame_view()->InsertClientView(
      new views::ClientView(widget_.get(), view_.get()));
  widget_->non_client_view()->frame_view()->SetLayoutManager(
      std::make_unique<views::FillLayout>());
  widget_->Show();
  widget_->SetCapture(widget_->client_view());
}

content::PreloadingEligibility PreviewTab::IsPrerender2Supported(
    content::WebContents& web_contents) {
  return content::PreloadingEligibility::kPreloadingDisabled;
}

bool PreviewTab::IsInPreviewMode() const {
  return is_in_preview_mode_ &&
         base::FeatureList::IsEnabled(blink::features::kLinkPreviewNavigation);
}

// TODO(b:305000959): Write a browser test once the preview_test_util is landed.
void PreviewTab::Activate(base::OnceClosure completion_callback) {
  view_->GetWebContents()->ActivatePreviewPage(base::TimeTicks::Now(),
                                               std::move(completion_callback));

  // Reset the flag here as we run several checks whether the WebContents is
  // actually running in preview mode in the call above.
  is_in_preview_mode_ = false;
}

void PreviewTab::CancelPreviewByMojoBinderPolicy(
    const std::string& interface_name) {
  // TODO(b:299240273): Navigate to an error page.
}
