// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/preview/preview_tab.h"

#include "base/debug/debugger.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/preloading/chrome_preloading.h"
#include "chrome/browser/preloading/prerender/prerender_utils.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/preloading_data.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

PreviewTab::PreviewTab(content::WebContents& parent, const GURL& url)
    : widget_(std::make_unique<views::Widget>()),
      view_(std::make_unique<views::WebView>(parent.GetBrowserContext())),
      url_(url) {
  // WebView setup.
  view_->GetWebContents()->SetDelegate(this);
  AttachTabHelpersForInit();

  // The attempt is attached to the parent WebContents that initiates the
  // Link-Preview.
  // TODO(b:292184832): Verify if this approach works fine with the LinkPreview
  // use-cases later. See the review comment at https://crrev.com/c/4886428.
  auto* preloading_data =
      content::PreloadingData::GetOrCreateForWebContents(&parent);
  content::PreloadingAttempt* preloading_attempt =
      preloading_data->AddPreloadingAttempt(
          chrome_preloading_predictor::kLinkPreview,
          content::PreloadingType::kLinkPreview,
          content::PreloadingData::GetSameURLMatcher(url),
          parent.GetPrimaryMainFrame()->GetPageUkmSourceId());

  // TODO(b:292184832): Need yet another API to trigger prerendering with more
  // navigation related information, e.g. referrer, etc, to mimic anchor
  // navigation.
  prerender_handle_ = view_->GetWebContents()->StartPrerendering(
      url, content::PrerenderTriggerType::kEmbedder,
      prerender_utils::kLinkPreviewMetricsSuffix,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_TOPLEVEL),
      content::PreloadingHoldbackStatus::kUnspecified, preloading_attempt);

  InitWindow(parent);
}

PreviewTab::~PreviewTab() = default;

void PreviewTab::Show() {
  // The page should be shown on activating a prerendered page.
  widget_->Show();
  view_->LoadInitialURL(url_);
  widget_->SetCapture(widget_->client_view());
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
}

content::PreloadingEligibility PreviewTab::IsPrerender2Supported(
    content::WebContents& web_contents) {
  return content::PreloadingEligibility::kEligible;
}
