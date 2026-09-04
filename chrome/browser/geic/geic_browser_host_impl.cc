// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/geic/geic_browser_host_impl.h"

#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "chrome/browser/geic/geic_pwc_manager.h"
#include "chrome/browser/geic/geic_tab_context_extraction_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tab_strip_tracker.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/tab_change_type.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "google_apis/gaia/gaia_urls.h"
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
  GURL configured_guest_url = GeicPwcManager::GetConfiguredGuestURL(nullptr);
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

  mojom::TabContextOptions request_options;
  if (options) {
    request_options = *options;
  }

  auto wrapped_callback = std::move(callback).Then(
      base::BindOnce(&GeicBrowserHostImpl::ResetTabContextRunner,
                     weak_ptr_factory_.GetWeakPtr()));

  tab_context_runner_ = std::make_unique<TabContextExtractionRunner>(
      weak_ptr_factory_.GetWeakPtr(), validated_tab.contents,
      std::move(validated_tab.metadata), std::move(request_options),
      std::move(wrapped_callback));
  tab_context_runner_->Run();
}

void GeicBrowserHostImpl::ResetTabContextRunner() {
  if (tab_context_runner_) {
    base::SequencedTaskRunner::GetCurrentDefault()->DeleteSoon(
        FROM_HERE, std::move(tab_context_runner_));
  }
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
