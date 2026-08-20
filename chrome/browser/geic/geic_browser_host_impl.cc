// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geic/geic_browser_host_impl.h"

#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/containers/to_vector.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/types/expected.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/tabs/public/tab_interface.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"

namespace geic {

namespace {

std::string_view RejectionKindToReasonString(RejectionKind kind) {
  switch (kind) {
    case RejectionKind::kNone:
      return "";
    case RejectionKind::kProfileInvalid:
      return "profile null or off-the-record";
    case RejectionKind::kNoBrowser:
      return "no active browser window or profile mismatch";
    case RejectionKind::kNoActiveTab:
      return "no active tab in browser";
    case RejectionKind::kTabNotShareable:
      return "tab not valid for sharing (privileged or disallowed scheme)";
  }
}

mojom::GetTabContextError RejectionKindToTabContextError(RejectionKind kind) {
  switch (kind) {
    case RejectionKind::kNone:
    case RejectionKind::kNoActiveTab:
      return mojom::GetTabContextError::kTabClosed;
    case RejectionKind::kProfileInvalid:
    case RejectionKind::kNoBrowser:
      return mojom::GetTabContextError::kInternalError;
    case RejectionKind::kTabNotShareable:
      return mojom::GetTabContextError::kPermissionDenied;
  }
}

}  // namespace

// TODO(crbug.com/539909218): Glic's IsTabValidForSharing and the focused/pinned
// "expressed sharing intent" layer should eventually be shared rather than
// reimplemented once common sharing infrastructure is available.
bool IsTabValidForSharing(content::WebContents* wc) {
  if (!wc) {
    return false;
  }
  // Privileged content is served over https, so a scheme allowlist alone would
  // admit it — one privileged panel must not read another's contents. This case
  // did not exist before PWC.
  if (wc->IsPrivileged()) {
    return false;
  }
  return wc->GetLastCommittedURL().SchemeIsHTTPOrHTTPS();
}

GeicBrowserHostImpl::GeicBrowserHostImpl(Profile* profile) : profile_(profile) {
  tab_strip_tracker_ = std::make_unique<BrowserTabStripTracker>(this, this);
  tab_strip_tracker_->Init();
}

GeicBrowserHostImpl::~GeicBrowserHostImpl() {
  tab_strip_tracker_.reset();
}

bool GeicBrowserHostImpl::ShouldTrackBrowser(BrowserWindowInterface* browser) {
  return browser && browser->GetProfile() == profile_ &&
         !browser->GetProfile()->IsOffTheRecord();
}

void GeicBrowserHostImpl::BindBrowserHost(
    mojo::PendingReceiver<mojom::GeicBrowserHost> receiver) {
  receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void GeicBrowserHostImpl::RegisterClient(
    mojo::PendingRemote<mojom::GeicClient> client,
    RegisterClientCallback callback) {
  DVLOG(1) << "[geic] RegisterClient, client_valid=" << client.is_valid();
  client_remote_.reset();
  client_remote_.Bind(std::move(client));

  auto state = mojom::GeicInitialState::New();
  state->focused_tab_data = GetCurrentFocusedTabData();
  std::move(callback).Run(std::move(state));
}

void GeicBrowserHostImpl::NotifyFocusedTabChanged(
    mojom::FocusedTabDataPtr data) {
  if (client_remote_.is_bound()) {
    client_remote_->OnFocusedTabChanged(std::move(data));
  }
}

GeicBrowserHostImpl::ValidatedActiveTab
GeicBrowserHostImpl::GetValidatedActiveTab() {
  if (!profile_ || profile_->IsOffTheRecord()) {
    DVLOG(1) << "[geic] reject: profile null or off-the-record";
    return {.rejection = RejectionKind::kProfileInvalid};
  }
  BrowserWindowInterface* browser =
      active_browser_for_testing_
          ? active_browser_for_testing_.get()
          : GlobalBrowserCollection::GetInstance()->GetActiveBrowser();
  if (!browser || browser->GetProfile() != profile_) {
    DVLOG(1) << "[geic] reject: browser=" << browser << " profile_match="
             << (browser && browser->GetProfile() == profile_);
    return {.rejection = RejectionKind::kNoBrowser};
  }
  tabs::TabInterface* active_tab = browser->GetActiveTabInterface();
  if (!active_tab) {
    DVLOG(1) << "[geic] reject: no active tab in browser";
    return {.rejection = RejectionKind::kNoActiveTab};
  }
  content::WebContents* active_contents = active_tab->GetContents();
  if (!active_contents || !IsTabValidForSharing(active_contents)) {
    DVLOG(1) << "[geic] reject: url="
             << (active_contents ? active_contents->GetLastCommittedURL().spec()
                                 : "<null contents>")
             << " privileged="
             << (active_contents && active_contents->IsPrivileged());
    return {.rejection = RejectionKind::kTabNotShareable};
  }

  auto metadata = mojom::TabMetadata::New();
  metadata->tab_id =
      active_contents->GetPrimaryMainFrame()->GetFrameTreeNodeId().value();
  metadata->window_id = browser->GetSessionID().id();
  metadata->url = active_contents->GetLastCommittedURL();
  metadata->title = active_contents->GetTitle();
  metadata->is_active_in_window = true;

  favicon::ContentFaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(active_contents);
  if (favicon_driver) {
    gfx::Image favicon_image = favicon_driver->GetFavicon();
    if (!favicon_image.IsEmpty()) {
      SkBitmap bitmap = favicon_image.AsBitmap();
      if (!bitmap.drawsNothing()) {
        std::optional<std::vector<uint8_t>> png =
            gfx::PNGCodec::EncodeBGRASkBitmap(bitmap,
                                              /*discard_transparency=*/false);
        if (png.has_value() && !png->empty()) {
          metadata->favicon = std::move(*png);
        }
      }
    }
  }

  DVLOG(1) << "[geic] accept: url=" << metadata->url;
  return {active_contents, std::move(metadata), RejectionKind::kNone};
}

mojom::FocusedTabDataPtr GeicBrowserHostImpl::GetCurrentFocusedTabData() {
  auto validated = GetValidatedActiveTab();
  if (validated.contents && validated.metadata) {
    return mojom::FocusedTabData::NewFocusedTab(std::move(validated.metadata));
  }
  auto no_focus = mojom::NoFocusedTabData::New();
  no_focus->no_focus_reason =
      std::string(RejectionKindToReasonString(validated.rejection));
  return mojom::FocusedTabData::NewNoFocusedTabData(std::move(no_focus));
}

void GeicBrowserHostImpl::GetFocusedTab(GetFocusedTabCallback callback) {
  DVLOG(1) << "[geic] GetFocusedTab";
  std::move(callback).Run(GetCurrentFocusedTabData());
}

void GeicBrowserHostImpl::GetContextFromFocusedTab(
    mojom::TabContextOptionsPtr options,
    GetContextFromFocusedTabCallback callback) {
  DVLOG(1) << "[geic] GetContextFromFocusedTab";
  auto validated_tab = GetValidatedActiveTab();
  if (!validated_tab.contents) {
    std::move(callback).Run(base::unexpected(
        RejectionKindToTabContextError(validated_tab.rejection)));
    return;
  }

  content::RenderFrameHost* primary_main_frame =
      validated_tab.contents->GetPrimaryMainFrame();

  const bool include_screenshot = options && options->include_screenshot;
  const bool include_inner_text = options && options->include_inner_text;

  // Options fields:
  // - inner_text_bytes_limit: cap bytes of text extracted (0 selects browser
  // default)
  // - screenshot_max_width / screenshot_max_height: maximum bounds for capture
  //   (0 selects viewport dimension)
  gfx::Size target_screenshot_size;
  if (options && (options->screenshot_max_width > 0 ||
                  options->screenshot_max_height > 0)) {
    gfx::Size view_size;
    if (primary_main_frame->GetView()) {
      view_size = primary_main_frame->GetView()->GetVisibleViewportSize();
    }
    const int width = options->screenshot_max_width > 0
                          ? options->screenshot_max_width
                          : view_size.width();
    const int height = options->screenshot_max_height > 0
                           ? options->screenshot_max_height
                           : view_size.height();
    target_screenshot_size = gfx::Size(width, height);
  }

  auto data = mojom::TabContextData::New();
  data->metadata = std::move(validated_tab.metadata);

  const uint32_t inner_text_bytes_limit =
      options ? options->inner_text_bytes_limit : 0;

  if (include_inner_text) {
    GetInnerText(primary_main_frame, inner_text_bytes_limit, include_screenshot,
                 target_screenshot_size, std::move(data), std::move(callback));
  } else if (include_screenshot) {
    CaptureScreenshot(primary_main_frame, target_screenshot_size,
                      std::move(data), std::move(callback));
  } else {
    std::move(callback).Run(std::move(data));
  }
}

void GeicBrowserHostImpl::GetInnerText(
    content::RenderFrameHost* primary_main_frame,
    uint32_t inner_text_bytes_limit,
    bool capture_screenshot,
    gfx::Size screenshot_size,
    mojom::TabContextDataPtr data,
    GetContextFromFocusedTabCallback callback) {
  // TODO(crbug.com/539909218): Honour options->inner_text_bytes_limit at DOM
  // extraction time in content_extraction to avoid pulling unconstrained text.
  content_extraction::GetInnerText(
      *primary_main_frame,
      /*node_id=*/std::nullopt,
      base::BindOnce(&GeicBrowserHostImpl::DidGetInnerText,
                     weak_ptr_factory_.GetWeakPtr(),
                     primary_main_frame->GetWeakDocumentPtr(),
                     inner_text_bytes_limit, capture_screenshot,
                     screenshot_size, std::move(data), std::move(callback)));
}

void GeicBrowserHostImpl::DidGetInnerText(
    content::WeakDocumentPtr document_ptr,
    uint32_t inner_text_bytes_limit,
    bool capture_screenshot,
    gfx::Size screenshot_size,
    mojom::TabContextDataPtr data,
    GetContextFromFocusedTabCallback callback,
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  auto validated = GetValidatedActiveTab();
  // Defend against primary main frame document replacement / navigation during
  // async inner text extraction. We check whether `document_ptr` is still valid
  // and matches the current active document. Subframe navigations are
  // deliberately ignored because inner text is extracted from the primary main
  // frame only.
  if (!validated.contents || document_ptr.AsRenderFrameHostIfValid() !=
                                 validated.contents->GetPrimaryMainFrame()) {
    std::move(callback).Run(
        base::unexpected(mojom::GetTabContextError::kNavigationInProgress));
    return;
  }

  data->metadata = std::move(validated.metadata);
  if (result) {
    if (inner_text_bytes_limit > 0 &&
        result->inner_text.size() > inner_text_bytes_limit) {
      base::TruncateUTF8ToByteSize(result->inner_text, inner_text_bytes_limit,
                                   &result->inner_text);
    }
    data->inner_text = std::move(result->inner_text);
  }

  if (capture_screenshot) {
    CaptureScreenshot(validated.contents->GetPrimaryMainFrame(),
                      screenshot_size, std::move(data), std::move(callback));
  } else {
    std::move(callback).Run(std::move(data));
  }
}

void GeicBrowserHostImpl::CaptureScreenshot(
    content::RenderFrameHost* primary_main_frame,
    gfx::Size screenshot_size,
    mojom::TabContextDataPtr data,
    GetContextFromFocusedTabCallback callback) {
  content::RenderWidgetHostView* view = primary_main_frame->GetView();
  if (!view) {
    std::move(callback).Run(
        base::unexpected(mojom::GetTabContextError::kInternalError));
    return;
  }

  view->CopyFromSurface(
      gfx::Rect(), screenshot_size, base::TimeDelta(),
      base::BindOnce(&GeicBrowserHostImpl::DidCaptureScreenshot,
                     weak_ptr_factory_.GetWeakPtr(),
                     primary_main_frame->GetWeakDocumentPtr(), std::move(data),
                     std::move(callback)));
}

void GeicBrowserHostImpl::DidCaptureScreenshot(
    content::WeakDocumentPtr document_ptr,
    mojom::TabContextDataPtr data,
    GetContextFromFocusedTabCallback callback,
    const content::CopyFromSurfaceResult& result) {
  auto validated = GetValidatedActiveTab();
  // Defend against primary main frame document replacement / navigation during
  // async screenshot capture. We check whether `document_ptr` is still valid
  // and matches the current active document. Subframe navigations are
  // deliberately ignored because inner text is extracted from the primary main
  // frame only.
  if (!validated.contents || document_ptr.AsRenderFrameHostIfValid() !=
                                 validated.contents->GetPrimaryMainFrame()) {
    std::move(callback).Run(
        base::unexpected(mojom::GetTabContextError::kNavigationInProgress));
    return;
  }

  SkBitmap bitmap = result.value_or(viz::CopyOutputBitmapWithMetadata()).bitmap;
  if (!bitmap.drawsNothing()) {
    std::optional<std::vector<uint8_t>> jpeg_data =
        gfx::JPEGCodec::Encode(bitmap, 80);
    if (jpeg_data.has_value()) {
      data->screenshot_data = std::move(*jpeg_data);
      data->screenshot_mime_type = "image/jpeg";
    }
  }

  std::move(callback).Run(std::move(data));
}

void GeicBrowserHostImpl::ClosePanel() {
  BrowserWindowInterface* browser =
      GlobalBrowserCollection::GetInstance()->GetActiveBrowser();
  if (browser && browser->GetFeatures().side_panel_ui()) {
    browser->GetFeatures().side_panel_ui()->Close();
  }
}

void GeicBrowserHostImpl::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (selection.active_tab_changed() && client_remote_.is_bound()) {
    DVLOG(1) << "[geic] OnTabStripModelChanged active tab changed";
    client_remote_->OnFocusedTabChanged(GetCurrentFocusedTabData());
  }
}

void GeicBrowserHostImpl::OnTabChangedAt(tabs::TabInterface* tab,
                                         TabChangeType change_type) {
  if (change_type == TabChangeType::kAll && client_remote_.is_bound()) {
    DVLOG(1) << "[geic] OnTabChangedAt";
    client_remote_->OnFocusedTabChanged(GetCurrentFocusedTabData());
  }
}

}  // namespace geic
