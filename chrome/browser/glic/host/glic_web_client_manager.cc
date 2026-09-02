// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_web_client_manager.h"

#include <utility>

#include "base/check_deref.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/glic/host/glic_web_client_handler.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {

namespace {

mojom::GuestPageType GetGuestPageType(const GURL& url) {
  if (IsAdminBlockedUrl(url)) {
    return mojom::GuestPageType::kDisabledByAdmin;
  }
  if (url.DomainIs("login.corp.google.com") ||
      url.DomainIs("accounts.google.com") ||
      url.DomainIs("accounts.googlers.com") ||
      url.DomainIs("gaiastaging.corp.google.com")) {
    return mojom::GuestPageType::kLogin;
  }
  if (url.path().starts_with("/sorry/")) {
    return mojom::GuestPageType::kGuestError;
  }
  return mojom::GuestPageType::kRegular;
}

// LINT.IfChange(GlicWebviewExitReason)
enum class GlicWebviewExitReason {
  kNormal = 0,
  kAbnormal = 1,
  kCrashed = 2,
  kKilled = 3,
  kOomKilled = 4,
  kOom = 5,
  kFailedToLaunch = 6,
  kIntegrityFailure = 7,
  kUnknown = 8,
  kMaxValue = kUnknown,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicWebviewExitReason)

GlicWebviewExitReason TerminationStatusToExitReason(
    base::TerminationStatus status) {
  switch (status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      return GlicWebviewExitReason::kNormal;
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      return GlicWebviewExitReason::kAbnormal;
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      return GlicWebviewExitReason::kCrashed;
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      return GlicWebviewExitReason::kKilled;
    case base::TERMINATION_STATUS_OOM:
      return GlicWebviewExitReason::kOom;
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      return GlicWebviewExitReason::kFailedToLaunch;
#if BUILDFLAG(IS_WIN)
    case base::TERMINATION_STATUS_INTEGRITY_FAILURE:
      return GlicWebviewExitReason::kIntegrityFailure;
#endif
    default:
      return GlicWebviewExitReason::kUnknown;
  }
}

}  // namespace

GlicWebClientManager::GlicWebClientManager() = default;

GlicWebClientManager::~GlicWebClientManager() = default;

void GlicWebClientManager::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

void GlicWebClientManager::AttachToHost(Host* host) {
  CHECK(!host_);
  CHECK(host);
  host_ = host;
  if (pending_web_client_receiver_.is_valid()) {
    CreateWebClient(std::move(pending_web_client_receiver_));
  }
}

void GlicWebClientManager::AttachGuestContents(
    content::WebContents* guest_contents) {
  DVLOG(1) << "Glic [WebClientManager] AttachGuestContents " << guest_contents;
  has_navigation_committed_ = false;
  Observe(guest_contents);
}

void GlicWebClientManager::OnGuestNavigationBlocked(
    mojom::GuestPageType page_type) {
  DVLOG(1) << "Glic [WebClientManager] OnGuestNavigationBlocked " << page_type;
}

content::RenderFrameHost* GlicWebClientManager::GetGuestMainFrame() const {
  return web_contents() ? web_contents()->GetPrimaryMainFrame() : nullptr;
}

content::WebContents* GlicWebClientManager::web_client_contents() const {
  return web_contents();
}

void GlicWebClientManager::SetPendingWebClientReceiver(
    mojo::PendingReceiver<glic::mojom::WebClientHandler> web_client_receiver) {
  if (host_) {
    CreateWebClient(std::move(web_client_receiver));
  } else {
    pending_web_client_receiver_ = std::move(web_client_receiver);
  }
}

void GlicWebClientManager::CreateWebClient(
    mojo::PendingReceiver<glic::mojom::WebClientHandler> web_client_receiver) {
  CHECK(host_);
  base::UmaHistogramEnumeration("Glic.Host.WebClientLifecycleEvent",
                                GlicWebClientLifecycleEvent::kCreated);
  if (delegate_) {
    delegate_->OnWebClientCreated();
  }
  if (web_client_owned_) {
    UnsetWebClient();
  }
  web_client_owned_ = MakeGlicWebClient(
      host_, host_->profile(), std::move(web_client_receiver),
      base::BindOnce(&GlicWebClientManager::UnsetWebClient,
                     // Safe, web_client_owned_ is owned by this.
                     base::Unretained(this), std::nullopt),
      base::BindRepeating(&GlicWebClientManager::OnWebClientStateChanged,
                          // Safe, web_client_owned_ is owned by this.
                          base::Unretained(this)));
}

void GlicWebClientManager::OnWebClientStateChanged(
    mojom::WebClientState state) {
  if (delegate_) {
    delegate_->OnWebClientStateChanged(state);
  }
  if (host_) {
    host_->OnWebClientStateChanged(state);
  }
}

void GlicWebClientManager::WebClientInitialized() {
  CHECK(web_client_owned_);
  web_client_ = web_client_owned_.get();
  base::UmaHistogramEnumeration("Glic.Host.WebClientLifecycleEvent",
                                GlicWebClientLifecycleEvent::kInitialized);
}

void GlicWebClientManager::UnsetWebClient(
    std::optional<GlicWebClientLifecycleEvent> event) {
  if (!web_client_owned_) {
    return;
  }
  DVLOG(1) << "Glic [WebClientManager] UnsetWebClient, had_access="
           << (web_client_owned_ ? "true" : "false");
  bool had_web_client = (web_client_ != nullptr);
  base::UmaHistogramEnumeration(
      "Glic.Host.WebClientLifecycleEvent",
      event.value_or(
          had_web_client
              ? GlicWebClientLifecycleEvent::kDisconnectedAfterInitialization
              : GlicWebClientLifecycleEvent::
                    kDisconnectedBeforeInitialization));
  web_client_ = nullptr;
  web_client_owned_.reset();
  if (host_) {
    host_->OnGuestWebClientCleared(had_web_client);
  }
}

void GlicWebClientManager::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  if (delegate_) {
    delegate_->OnGuestNavigationStarted();
  }
}

void GlicWebClientManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInMainFrame() ||
      !navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  content::RenderFrameHost* guest_main_frame =
      navigation_handle->GetRenderFrameHost();
  DVLOG(1) << "Glic [WebClientManager] DidFinishNavigation, url="
           << guest_main_frame->GetLastCommittedURL();
  bool is_api_allowed =
      IsOriginAllowedGlicApi(guest_main_frame->GetLastCommittedOrigin());
  mojom::GuestPageType page_type =
      GetGuestPageType(guest_main_frame->GetLastCommittedURL());
  bool is_initial_commit = !has_navigation_committed_;
  has_navigation_committed_ = true;

  // Note, this notifies the WebUI page about the navigation, which may hide or
  // show the guest contents.
  if (delegate_) {
    delegate_->OnGuestNavigated(guest_main_frame->GetLastCommittedURL(),
                                is_api_allowed, page_type, is_initial_commit);
  }
  if (web_client_owned_) {
    UnsetWebClient(GlicWebClientLifecycleEvent::kDisconnectedOnNavigation);
  }
}

void GlicWebClientManager::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  DVLOG(1)
      << "Glic [WebClientManager] PrimaryMainFrameRenderProcessGone, status="
      << std::to_underlying(status);
  base::UmaHistogramEnumeration("Glic.Session.WebClientCrash.ExitReason",
                                TerminationStatusToExitReason(status));
  if (web_client_owned_) {
    UnsetWebClient(GlicWebClientLifecycleEvent::kDisconnectedOnProcessGone);
  }
  if (status != base::TERMINATION_STATUS_NORMAL_TERMINATION) {
    base::RecordAction(base::UserMetricsAction("GlicSessionWebClientCrash"));
    if (delegate_) {
      delegate_->OnGuestProcessGone(status);
    }
  }
}

}  // namespace glic
