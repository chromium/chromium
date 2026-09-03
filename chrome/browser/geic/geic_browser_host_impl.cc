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
#include "chrome/browser/geic/geic_pwc_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/content_extraction/content/browser/inner_text.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "components/viz/common/frame_sinks/copy_output_result.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_urls.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/origin.h"

namespace geic {

namespace {

bool IsSignInURLAllowed(const GURL& url, Profile* profile) {
  if (!url.is_valid() || !url.SchemeIs(url::kHttpsScheme)) {
    return false;
  }
  url::Origin origin = url::Origin::Create(url);
  if (origin == GaiaUrls::GetInstance()->gaia_origin()) {
    return true;
  }
  GURL configured_guest_url = GeicPwcManager::GetConfiguredGuestURL();
  if (!configured_guest_url.is_empty() &&
      origin == url::Origin::Create(configured_guest_url)) {
    return true;
  }
  if (profile) {
    if (auto* manager = GeicPwcManager::GetOrCreateForProfile(profile)) {
      if (!manager->guest_url().is_empty() &&
          origin == url::Origin::Create(manager->guest_url())) {
        return true;
      }
    }
  }
  return false;
}

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

GeicBrowserHostImpl::GeicBrowserHostImpl(tabs::TabInterface* tab)
    : tab_(tab), profile_(tab ? tab->GetProfile() : nullptr) {
  if (tab_) {
    will_detach_subscription_ = tab_->RegisterWillDetach(base::BindRepeating(
        &GeicBrowserHostImpl::OnTabWillDetach, base::Unretained(this)));
  }
  tab_strip_tracker_ = std::make_unique<BrowserTabStripTracker>(this, this);
  tab_strip_tracker_->Init();
}

GeicBrowserHostImpl::~GeicBrowserHostImpl() {
  tab_strip_tracker_.reset();
}

void GeicBrowserHostImpl::OnTabWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  if (reason == tabs::TabInterface::DetachReason::kDelete) {
    tab_ = nullptr;
    will_detach_subscription_ = {};
  }
}

bool GeicBrowserHostImpl::ShouldTrackBrowser(BrowserWindowInterface* browser) {
  return browser && profile_ && browser->GetProfile() == profile_ &&
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
  if (!tab_) {
    DVLOG(1) << "[geic] reject: tab is null";
    return {.rejection = RejectionKind::kNoActiveTab};
  }
  BrowserWindowInterface* browser_window = tab_->GetBrowserWindowInterface();
  if (!browser_window) {
    DVLOG(1) << "[geic] reject: tab is not attached to a browser window";
    return {.rejection = RejectionKind::kNoBrowser};
  }
  Profile* profile = browser_window->GetProfile();
  if (!profile || profile->IsOffTheRecord()) {
    DVLOG(1) << "[geic] reject: profile null or off-the-record";
    return {.rejection = RejectionKind::kProfileInvalid};
  }
  tabs::TabInterface* active_tab = browser_window->GetActiveTabInterface();
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
  metadata->window_id = browser_window->GetSessionID().id();
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

void GeicBrowserHostImpl::OpenSignInTab(const GURL& signin_url) {
  DVLOG(1) << "[geic] OpenSignInTab: " << signin_url;
  if (!tab_) {
    return;
  }
  BrowserWindowInterface* browser_window = tab_->GetBrowserWindowInterface();
  if (!browser_window) {
    return;
  }
  Profile* profile = browser_window->GetProfile();
  if (!profile || profile->IsOffTheRecord()) {
    return;
  }
  if (!IsSignInURLAllowed(signin_url, profile)) {
    DVLOG(1) << "[geic] OpenSignInTab rejected disallowed sign-in URL: "
             << signin_url;
    return;
  }
  if (signin_web_contents_) {
    if (auto* tab_strip = browser_window->GetTabStripModel()) {
      int index = tab_strip->GetIndexOfWebContents(signin_web_contents_.get());
      if (index != TabStripModel::kNoTab) {
        tab_strip->ActivateTabAt(index);
        return;
      }
    }
  }
  if (auto* active_tab = browser_window->GetActiveTabInterface()) {
    original_tab_ = active_tab->GetWeakPtr();
  }
  NavigateParams params(browser_window, signin_url,
                        ui::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  params.user_gesture = false;
  Navigate(&params);
  if (params.navigated_or_inserted_contents) {
    signin_web_contents_ = params.navigated_or_inserted_contents->GetWeakPtr();
    has_opened_signin_tab_ = true;
  }
}

void GeicBrowserHostImpl::CloseSignInTab(CloseSignInTabCallback callback) {
  DVLOG(1) << "[geic] CloseSignInTab, signin_web_contents="
           << signin_web_contents_.get()
           << ", has_opened_signin_tab=" << has_opened_signin_tab_;
  if (!has_opened_signin_tab_) {
    std::move(callback).Run(mojom::CloseSignInTabResult::kNoSignInTab);
    return;
  }

  has_opened_signin_tab_ = false;

  if (!signin_web_contents_) {
    original_tab_.reset();
    std::move(callback).Run(mojom::CloseSignInTabResult::kAlreadyClosed);
    return;
  }

  content::WebContents* signin_contents = signin_web_contents_.get();
  signin_web_contents_.reset();
  bool closed = false;
  BrowserWindowInterface* browser_window =
      tab_ ? tab_->GetBrowserWindowInterface() : nullptr;
  if (browser_window) {
    if (auto* tab_strip = browser_window->GetTabStripModel()) {
      int index = tab_strip->GetIndexOfWebContents(signin_contents);
      if (index != TabStripModel::kNoTab) {
        tab_strip->CloseWebContentsAt(
            index, TabCloseTypes::CLOSE_USER_GESTURE |
                       TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
        closed = true;
      }
    }
  }
  if (!closed) {
    signin_contents->Close();
  }
  if (original_tab_ && original_tab_->GetBrowserWindowInterface()) {
    if (auto* tab_strip =
            original_tab_->GetBrowserWindowInterface()->GetTabStripModel()) {
      int index = tab_strip->GetIndexOfTab(original_tab_.get());
      if (index != TabStripModel::kNoTab) {
        tab_strip->ActivateTabAt(index);
      }
    }
  }
  original_tab_.reset();
  std::move(callback).Run(mojom::CloseSignInTabResult::kSuccess);
}

void GeicBrowserHostImpl::ClosePanel() {
  BrowserWindowInterface* browser =
      tab_ ? tab_->GetBrowserWindowInterface() : nullptr;
  if (browser && SidePanelUI::From(browser)) {
    SidePanelUI::From(browser)->Close();
  }
}

void GeicBrowserHostImpl::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  BrowserWindowInterface* current_browser =
      tab_ ? tab_->GetBrowserWindowInterface() : nullptr;
  if (!current_browser ||
      current_browser->GetTabStripModel() != tab_strip_model) {
    return;
  }
  if (selection.active_tab_changed() && client_remote_.is_bound()) {
    DVLOG(1) << "[geic] OnTabStripModelChanged active tab changed";
    client_remote_->OnFocusedTabChanged(GetCurrentFocusedTabData());
  }
}

void GeicBrowserHostImpl::OnTabChangedAt(tabs::TabInterface* tab,
                                         TabChangeType change_type) {
  BrowserWindowInterface* current_browser =
      tab_ ? tab_->GetBrowserWindowInterface() : nullptr;
  if (!current_browser || tab->GetBrowserWindowInterface() != current_browser) {
    return;
  }
  if (change_type == TabChangeType::kAll && client_remote_.is_bound()) {
    DVLOG(1) << "[geic] OnTabChangedAt";
    client_remote_->OnFocusedTabChanged(GetCurrentFocusedTabData());
  }
}

}  // namespace geic
