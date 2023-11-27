// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preloading/preview/preview_tab.h"

#include "base/features.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

namespace {

content::WebContents::CreateParams CreateWebContentsCreateParams(
    content::BrowserContext* context,
    content::WebContentsDelegate* delegate) {
  CHECK(context);
  CHECK(delegate);
  content::WebContents::CreateParams params(context);
  params.delegate = delegate;
  return params;
}

}  // namespace

class PreviewTab::PreviewWidget final : public views::Widget {
 public:
  explicit PreviewWidget(PreviewManager* preview_manager)
      : preview_manager_(preview_manager) {}

 private:
  // views::Widet implementation:
  void OnMouseEvent(ui::MouseEvent* event) override {
    auto rect = GetClientAreaBoundsInScreen();
    // Check the event occurred on this widget.
    // Note that `event->location()` is relative to the origin of widget.
    bool is_event_for_preview_window =
        0 <= event->location().x() &&
        event->location().x() <= rect.size().width() &&
        0 <= event->location().y() &&
        event->location().y() <= rect.size().height();

    // Tentative trigger for open-in-new-tab: Middle click on preview.
    if (is_event_for_preview_window && event->type() == ui::ET_MOUSE_RELEASED &&
        event->IsMiddleMouseButton()) {
      event->SetHandled();
      preview_manager_->PromoteToNewTab();
      return;
    }

    if (!is_event_for_preview_window &&
        event->type() == ui::ET_MOUSE_RELEASED) {
      event->SetHandled();
      preview_manager_->Cancel();
      return;
    }

    views::Widget::OnMouseEvent(event);
  }

  // Outlives because `PreviewManager` has `PreviewTab` and `PreviewTab` has
  // `PreviweWidget`.
  raw_ptr<PreviewManager> preview_manager_;
};

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

PreviewTab::PreviewTab(PreviewManager* preview_manager,
                       content::WebContents& initiator_web_contents,
                       const GURL& url)
    : web_contents_(content::WebContents::Create(CreateWebContentsCreateParams(
          initiator_web_contents.GetBrowserContext(),
          this))),
      widget_(std::make_unique<PreviewWidget>(preview_manager)),
      view_(std::make_unique<views::WebView>(nullptr)),
      url_(url) {
  CHECK(base::FeatureList::IsEnabled(blink::features::kLinkPreview));

  // WebView setup.
  view_->SetWebContents(web_contents_.get());

  AttachTabHelpersForInit();
  // Our observer should be created after ZoomController is created in
  // AttachTabHelpersForInit() above to ensure our DidFinishNavigation is called
  // after ZoomController::DidFinishNavigation resets zoom settings. This is
  // because the observer invocation order depends on its registration order.
  observer_ = std::make_unique<WebContentsObserver>(web_contents_.get());

  // TODO(b:292184832): Ensure if we provide enough information to perform an
  // equivalent navigation with a link navigation.
  view_->LoadInitialURL(url_);

  InitWindow(initiator_web_contents);
}

PreviewTab::~PreviewTab() = default;

base::WeakPtr<content::WebContents> PreviewTab::GetWebContents() {
  if (!web_contents_) {
    return nullptr;
  }

  return web_contents_->GetWeakPtr();
}

void PreviewTab::AttachTabHelpersForInit() {
  content::WebContents* web_contents = web_contents_.get();

  // TODO(b:291867757): Audit TabHelpers and determine when
  // (initiation/promotion) we should attach each of them.
  zoom::ZoomController::CreateForWebContents(web_contents);
  SecurityStateTabHelper::CreateForWebContents(web_contents);
  chrome::InitializePageLoadMetricsForWebContents(web_contents);
}

void PreviewTab::InitWindow(content::WebContents& initiator_web_contents) {
  // All details here are tentative until we fix the details of UI.
  //
  // TODO(go/launch/4269184): Revisit it later.

  views::Widget::InitParams params;
  // TODO(b:292184832): Create with own buttons
  params.type = views::Widget::InitParams::TYPE_WINDOW;
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.z_order = ui::ZOrderLevel::kFloatingWindow;
  const gfx::Rect rect = initiator_web_contents.GetContainerBounds();
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
  return true;
}

void PreviewTab::PromoteToNewTab(content::WebContents& initiator_web_contents) {
  view_->SetWebContents(nullptr);
  view_ = nullptr;

  auto web_contents = web_contents_->GetWeakPtr();

  // This force-set zoom factor 1 and don't respect per-site settings.
  //
  // TODO(b:308061954): Implement better zoom and fix this.
  auto* zoom_controller =
      zoom::ZoomController::FromWebContents(web_contents_.get());
  const double level = blink::PageZoomFactorToZoomLevel(1.0f);
  zoom_controller->SetZoomLevel(level);
  zoom_controller->SetZoomMode(
      zoom::ZoomController::ZoomMode::ZOOM_MODE_DEFAULT);

  TabHelpers::AttachTabHelpers(web_contents_.get());

  // Detach WebContentsDelegate before passing WebContents to another
  // WebContentsDelegate. It would be not necessary, but it's natural because
  // the other paths do, e.g. TabDragController::DetachAndAttachToNewContext,
  // which moves a tab from Browser to another Browser.
  web_contents_->SetDelegate(nullptr);

  // Pass WebContents to Browser.
  WebContentsDelegate* delegate = initiator_web_contents.GetDelegate();
  CHECK(delegate);
  blink::mojom::WindowFeaturesPtr window_features =
      blink::mojom::WindowFeatures::New();
  delegate->AddNewContents(/*source*/ nullptr,
                           /*new_contents*/ std::move(web_contents_),
                           /*target_url*/ url_,
                           WindowOpenDisposition::NEW_FOREGROUND_TAB,
                           *window_features,
                           /*user_gesture*/ true,
                           /*was_blocked*/ nullptr);

  Activate(web_contents);
}

void PreviewTab::Activate(base::WeakPtr<content::WebContents> web_contents) {
  CHECK(web_contents);
  web_contents->ActivatePreviewPage();
}

void PreviewTab::CancelPreviewByMojoBinderPolicy(
    const std::string& interface_name) {
  // TODO(b:299240273): Navigate to an error page.
}
