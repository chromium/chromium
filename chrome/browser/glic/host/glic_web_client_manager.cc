// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_web_client_manager.h"

#include <optional>
#include <utility>

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/time/time.h"
#include "chrome/browser/glic/host/glic_guest_actionable_mark_observer.h"
#include "chrome/browser/glic/host/glic_web_client_handler.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/pwc/privileged_web_contents.h"
#include "chrome/browser/pwc/pwc_component_policy.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {

BASE_FEATURE(kGlicShowSecurityInterstitials,
             FEATURE_ENABLED_BY_DEFAULT_ALL_PLATFORMS);

namespace {

// Classifies URLs into GuestPageType.
// Reverse proxy authentication paths (e.g. `/_up` or `/_up/...`) and SSO
// login endpoints are classified as kLogin so the guest WebContents is
// displayed directly to the user to authenticate rather than triggering
// an error overlay or treating them as a broken web client.
mojom::GuestPageType GetGuestPageType(const GURL& url) {
  if (IsAdminBlockedUrl(url)) {
    return mojom::GuestPageType::kDisabledByAdmin;
  }
  const std::string_view path = url.path();
  if (url.DomainIs("login.corp.google.com") ||
      url.DomainIs("accounts.google.com") ||
      url.DomainIs("accounts.googlers.com") ||
      url.DomainIs("gaiastaging.corp.google.com") || path == "/_up" ||
      path.starts_with("/_up/")) {
    return mojom::GuestPageType::kLogin;
  }
  if (path.starts_with("/sorry/")) {
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

class GlicWebClientManager::Metrics {
 public:
  Metrics() = default;
  ~Metrics() = default;

  void OnWebClientCreated() {
    base::UmaHistogramEnumeration("Glic.Host.WebClientLifecycleEvent",
                                  GlicWebClientLifecycleEvent::kCreated);
  }

  void OnWebClientInitialized() {
    base::UmaHistogramEnumeration("Glic.Host.WebClientLifecycleEvent",
                                  GlicWebClientLifecycleEvent::kInitialized);
  }

  void OnWebClientDisconnected(std::optional<GlicWebClientLifecycleEvent> event,
                               bool had_web_client) {
    base::UmaHistogramEnumeration(
        "Glic.Host.WebClientLifecycleEvent",
        event.value_or(
            had_web_client
                ? GlicWebClientLifecycleEvent::kDisconnectedAfterInitialization
                : GlicWebClientLifecycleEvent::
                      kDisconnectedBeforeInitialization));
  }

  void OnGuestNavigation(mojom::GuestPageType page_type, bool is_api_allowed) {
    if (page_type != mojom::GuestPageType::kRegular || !is_api_allowed) {
      has_error_or_crash_ = true;
    }
  }

  void OnProcessGone(base::TerminationStatus status) {
    has_error_or_crash_ = true;
    base::UmaHistogramEnumeration("Glic.Contents.WebClientCrash.ExitReason",
                                  TerminationStatusToExitReason(status));
    if (status != base::TERMINATION_STATUS_NORMAL_TERMINATION) {
      base::RecordAction(base::UserMetricsAction("GlicSessionWebClientCrash"));
    }
  }

  void OnWebClientStateChanged(mojom::WebClientState state) {
    if (state == mojom::WebClientState::kWarmed) {
      MaybeRecordTimeToWarmed();
    }
  }

  void MaybeRecordTimeToWarmed() {
    if (has_recorded_time_to_warmed_ || has_error_or_crash_) {
      return;
    }
    has_recorded_time_to_warmed_ = true;
    base::TimeDelta duration = base::TimeTicks::Now() - creation_time_;
    base::UmaHistogramCustomTimes("Glic.Contents.TimeToWarmed", duration,
                                  base::Milliseconds(1), base::Seconds(60), 50);
  }

 private:
  const base::TimeTicks creation_time_ = base::TimeTicks::Now();
  bool has_recorded_time_to_warmed_ = false;
  bool has_error_or_crash_ = false;
};

// Tracks guest presentation state for `Glic.Instance.TimeToActionable`.
//
// The guest page emits `performance.mark("gemini.interactive")` when its UI is
// ready for input. Once the guest is attached to a `Host`, shown in the panel
// (`guest_mounted_`), and has emitted that mark, this class waits for the
// renderer to submit one compositor frame (`InsertVisualStateCallback`) and
// then notifies `GlicInstanceMetrics::OnGuestFrameSubmitted()`.
class GlicWebClientManager::GuestPresentationTracker {
 public:
  explicit GuestPresentationTracker(GlicWebClientManager& manager)
      : manager_(manager) {}
  ~GuestPresentationTracker() = default;

  GuestPresentationTracker(const GuestPresentationTracker&) = delete;
  GuestPresentationTracker& operator=(const GuestPresentationTracker&) = delete;

  void OnAttachedToHost(GlicInstanceMetrics& instance_metrics) {
    CHECK(!instance_metrics_);
    instance_metrics_ = &instance_metrics;
    visual_state_weak_ptr_factory_.InvalidateWeakPtrs();
    instance_metrics_->OnGuestReset();
    if (actionable_mark_time_) {
      // Replay a mark that fired while warming before `AttachToHost()`.
      instance_metrics_->OnGuestActionableMarkObserved(*actionable_mark_time_);
    }
    MaybeRequestVisualStateCallback();
  }

  void ObserveGuestContents(content::WebContents& guest_contents) {
    // Drop any mark callback bound to a previous `guest_contents` (in
    // `<webview>` mode, the old guest WebContents can stay alive until GC after
    // a new `<webview>` is attached). This is intentionally not invalidated in
    // `Reset()` on navigation, because `ObserveGlicGuestActionableMark`
    // attaches a WebContents-scoped observer once per `guest_contents` and
    // reuses this callback across navigations of that WebContents.
    guest_mark_weak_ptr_factory_.InvalidateWeakPtrs();
    Reset();
    ObserveGlicGuestActionableMark(
        guest_contents,
        base::BindRepeating(&GuestPresentationTracker::OnActionableMarkObserved,
                            guest_mark_weak_ptr_factory_.GetWeakPtr()));
  }

  void SetGuestMounted(bool mounted) {
    guest_mounted_ = mounted;
    if (!guest_mounted_) {
      visual_state_weak_ptr_factory_.InvalidateWeakPtrs();
      return;
    }
    MaybeRequestVisualStateCallback();
  }

  void Reset() {
    actionable_mark_time_.reset();
    visual_state_weak_ptr_factory_.InvalidateWeakPtrs();
    if (instance_metrics_) {
      instance_metrics_->OnGuestReset();
    }
  }

 private:
  void OnActionableMarkObserved(base::TimeTicks mark_time) {
    // Only the first mark per guest document counts.
    if (actionable_mark_time_) {
      return;
    }
    actionable_mark_time_ = mark_time;
    if (instance_metrics_) {
      instance_metrics_->OnGuestActionableMarkObserved(mark_time);
    }
    MaybeRequestVisualStateCallback();
  }

  void MaybeRequestVisualStateCallback() {
    if (!instance_metrics_ || !guest_mounted_ || !actionable_mark_time_ ||
        !instance_metrics_->IsWaitingForActionable() ||
        visual_state_weak_ptr_factory_.HasWeakPtrs()) {
      return;
    }
    content::RenderFrameHost* rfh = manager_->GetGuestMainFrame();
    if (!rfh || !rfh->IsRenderFrameLive()) {
      return;
    }
    // Wait for the renderer to submit a compositor frame that includes the DOM
    // state from the actionability mark.
    rfh->InsertVisualStateCallback(
        base::BindOnce(&GuestPresentationTracker::OnVisualStateCallback,
                       visual_state_weak_ptr_factory_.GetWeakPtr()));
  }

  void OnVisualStateCallback(bool updated) {
    visual_state_weak_ptr_factory_.InvalidateWeakPtrs();
    // `InsertVisualStateCallback` only passes `updated = false` when the
    // renderer widget is missing or destroyed before completing the request
    // (including synchronously when `!blink_frame_widget_`), so do not
    // immediately re-request here.
    if (!updated || !instance_metrics_ || !guest_mounted_ ||
        !actionable_mark_time_ ||
        !instance_metrics_->IsWaitingForActionable()) {
      return;
    }
    instance_metrics_->OnGuestFrameSubmitted();
  }

  const raw_ref<GlicWebClientManager> manager_;
  // Null while warming in `GlicWebContentsWarmingPool`; set once in
  // `OnAttachedToHost()`.
  raw_ptr<GlicInstanceMetrics> instance_metrics_ = nullptr;
  // True when the guest WebContents is shown in the panel (not hidden or
  // covered by a loading/error overlay).
  bool guest_mounted_ = false;
  // Timestamp of the first "gemini.interactive" mark for the current guest
  // document.
  std::optional<base::TimeTicks> actionable_mark_time_;

  base::WeakPtrFactory<GuestPresentationTracker> guest_mark_weak_ptr_factory_{
      this};
  // Invalidated to cancel an in-flight `InsertVisualStateCallback`.
  base::WeakPtrFactory<GuestPresentationTracker> visual_state_weak_ptr_factory_{
      this};
};

GlicWebClientManager::GlicWebClientManager()
    : metrics_(std::make_unique<Metrics>()),
      guest_presentation_tracker_(
          std::make_unique<GuestPresentationTracker>(*this)) {}

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
  guest_presentation_tracker_->OnAttachedToHost(host->instance_metrics());
}

void GlicWebClientManager::AttachGuestContents(
    content::WebContents* guest_contents) {
  CHECK(guest_contents);
  DVLOG(1) << "Glic [WebClientManager] AttachGuestContents " << guest_contents;
  has_navigation_committed_ = false;
  current_navigation_blocked_ = false;
  last_committed_page_type_.reset();
  last_committed_is_api_allowed_ = false;
  guest_main_page_loaded_ = false;
  if (base::FeatureList::IsEnabled(kGlicShowSecurityInterstitials)) {
    // Ensure SecurityInterstitialTabHelper is registered as a
    // WebContentsObserver before GlicWebClientManager so its
    // DidFinishNavigation runs first to update committed interstitial state.
    security_interstitials::SecurityInterstitialTabHelper::CreateForWebContents(
        guest_contents);
  }
  Observe(guest_contents);
  guest_presentation_tracker_->ObserveGuestContents(*guest_contents);
}

void GlicWebClientManager::SetGuestMounted(bool mounted) {
  guest_presentation_tracker_->SetGuestMounted(mounted);
}

void GlicWebClientManager::OnGuestNavigationBlocked(
    mojom::GuestPageType page_type) {
  if (!base::FeatureList::IsEnabled(kGlicShowSecurityInterstitials)) {
    DVLOG(1) << "Glic [WebClientManager] OnGuestNavigationBlocked "
             << page_type;
    return;
  }
  // A single blocked navigation can be observed by both
  // `GlicGuestNavigationThrottle` (during `WillStartRequest`) and
  // `DidFinishNavigation` (when `PwcNavigationThrottle` cancels the request).
  // Deduplicate so metrics and delegate notifications fire at most once per
  // navigation.
  if (current_navigation_blocked_) {
    return;
  }
  current_navigation_blocked_ = true;
  DVLOG(1) << "Glic [WebClientManager] OnGuestNavigationBlocked " << page_type;
  // If a web client is already connected (or warmed) on the currently committed
  // page, a cancelled navigation (e.g. a subframe attempting to navigate the
  // top frame to about:blank) leaves the primary main frame on its active
  // document and should not tear down the connected client or show the error
  // screen unless the navigation was explicitly blocked by admin policy.
  if ((web_client_owned_ || pending_web_client_receiver_.is_valid()) &&
      page_type != mojom::GuestPageType::kDisabledByAdmin) {
    if (delegate_) {
      // Restore the delegate's ready state that `DidStartNavigation()` cleared
      // to `GuestState::kLoading` when the cancelled navigation began.
      if (pending_web_client_receiver_.is_valid()) {
        delegate_->OnWebClientStateChanged(mojom::WebClientState::kWarmed);
      } else {
        delegate_->OnWebClientCreated();
        if (web_client_) {
          delegate_->OnWebClientStateChanged(web_client_->web_client_state());
        }
      }
    }
    return;
  }
  // When no web client is active (e.g. clicking "Go back" on an initial
  // security interstitial, or a blocked initial load) or when the navigation is
  // blocked by admin policy (`kDisabledByAdmin`), mirror `DidFinishNavigation`
  // for a failed navigation: reset state, record the navigation metric, notify
  // the delegate to transition the panel to the error UI, and disconnect any
  // active web client.
  guest_presentation_tracker_->Reset();
  pending_web_client_receiver_.reset();
  last_committed_page_type_ = page_type;
  last_committed_is_api_allowed_ = false;
  guest_main_page_loaded_ = false;
  metrics_->OnGuestNavigation(page_type, /*is_api_allowed=*/false);
  if (delegate_) {
    delegate_->OnGuestNavigated(
        GURL(), /*is_api_allowed=*/false, page_type,
        /*is_initial_commit=*/!has_navigation_committed_);
  }
  if (web_client_owned_) {
    UnsetWebClient(GlicWebClientLifecycleEvent::kDisconnectedOnNavigation);
  }
  if (host_) {
    host_->UpdateNativeLoadingState();
  }
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
    if (delegate_) {
      delegate_->OnWebClientStateChanged(mojom::WebClientState::kWarmed);
    }
    metrics_->MaybeRecordTimeToWarmed();
  }
}

void GlicWebClientManager::CreateWebClient(
    mojo::PendingReceiver<glic::mojom::WebClientHandler> web_client_receiver) {
  CHECK(host_);
  metrics_->OnWebClientCreated();
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
  metrics_->OnWebClientStateChanged(state);
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
  metrics_->OnWebClientInitialized();
}

void GlicWebClientManager::UnsetWebClient(
    std::optional<GlicWebClientLifecycleEvent> event) {
  if (!web_client_owned_) {
    return;
  }
  DVLOG(1) << "Glic [WebClientManager] UnsetWebClient, had_access="
           << (web_client_owned_ ? "true" : "false");
  bool had_web_client = (web_client_ != nullptr);
  metrics_->OnWebClientDisconnected(event, had_web_client);
  web_client_ = nullptr;
  web_client_owned_.reset();
  if (host_) {
    host_->OnGuestWebClientCleared(had_web_client);
  }
}

void GlicWebClientManager::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }
  current_navigation_blocked_ = false;
  last_committed_page_type_.reset();
  last_committed_is_api_allowed_ = false;
  guest_main_page_loaded_ = false;
  if (!base::FeatureList::IsEnabled(kGlicShowSecurityInterstitials)) {
    pending_web_client_receiver_.reset();
  }
  if (delegate_) {
    delegate_->OnGuestNavigationStarted();
  }
  if (host_) {
    host_->OnGuestNavigationStarted();
  }
}

void GlicWebClientManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  if (!navigation_handle->HasCommitted()) {
    if (base::FeatureList::IsEnabled(kGlicShowSecurityInterstitials)) {
      // In NoWebview mode, PwcNavigationThrottle cancels navigations to
      // disallowed origins (e.g. when a user clicks "Go back" on a security
      // interstitial and it navigates to a default safe page like about:blank
      // or chrome://newtab). Transition directly to the appropriate error
      // state.
      if (auto* privileged =
              pwc::PrivilegedWebContents::FromWebContents(web_contents())) {
        const GURL& url = navigation_handle->GetURL();
        if (!privileged->policy().IsNavigationAllowed(
                url::Origin::Create(url))) {
          OnGuestNavigationBlocked(IsAdminBlockedUrl(url)
                                       ? mojom::GuestPageType::kDisabledByAdmin
                                       : mojom::GuestPageType::kOriginBlocked);
        }
      }
    }
    return;
  }
  guest_presentation_tracker_->Reset();
  // Only drop a warmed guest's pending receiver once a new navigation commits
  // (or fails fatally in `OnGuestNavigationBlocked`), so a navigation cancelled
  // by a throttle while warming does not sever the warmed page's Mojo pipe.
  pending_web_client_receiver_.reset();
  content::RenderFrameHost* guest_main_frame =
      navigation_handle->GetRenderFrameHost();
  DVLOG(1) << "Glic [WebClientManager] DidFinishNavigation, url="
           << guest_main_frame->GetLastCommittedURL();
  bool is_api_allowed =
      IsOriginAllowedGlicApi(guest_main_frame->GetLastCommittedOrigin(),
                             guest_main_frame->GetBrowserContext());
  bool is_security_interstitial = false;
  if (base::FeatureList::IsEnabled(kGlicShowSecurityInterstitials) &&
      navigation_handle->IsErrorPage()) {
    auto* interstitial_helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            web_contents());
    is_security_interstitial =
        interstitial_helper &&
        interstitial_helper->IsInterstitialCommittedForFrame(
            navigation_handle->GetFrameTreeNodeId());
  }
  bool is_error_page =
      navigation_handle->IsErrorPage() ||
      (navigation_handle->GetResponseHeaders() &&
       navigation_handle->GetResponseHeaders()->response_code() >= 400);
  mojom::GuestPageType page_type =
      GetGuestPageType(guest_main_frame->GetLastCommittedURL());
  if (is_security_interstitial) {
    page_type = mojom::GuestPageType::kSecurityInterstitial;
  } else if (is_error_page) {
    page_type = mojom::GuestPageType::kLoadError;
  }
  last_committed_page_type_ = page_type;
  last_committed_is_api_allowed_ = is_api_allowed;
  guest_main_page_loaded_ = false;
  metrics_->OnGuestNavigation(page_type, is_api_allowed);
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
  if (host_) {
    host_->UpdateNativeLoadingState();
  }
}

void GlicWebClientManager::DocumentOnLoadCompletedInPrimaryMainFrame() {
  if (base::FeatureList::IsEnabled(features::kGlicSsr) &&
      features::kGlicShowLoaderUntilMainPageOnload.Get() &&
      last_committed_page_type_ == mojom::GuestPageType::kRegular &&
      last_committed_is_api_allowed_) {
    guest_main_page_loaded_ = true;
    if (delegate_) {
      delegate_->OnGuestMainPageLoaded();
    }
    if (host_) {
      host_->UpdateNativeLoadingState();
    }
  }
}

void GlicWebClientManager::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  last_committed_page_type_.reset();
  last_committed_is_api_allowed_ = false;
  guest_main_page_loaded_ = false;
  metrics_->OnProcessGone(status);
  DVLOG(1)
      << "Glic [WebClientManager] PrimaryMainFrameRenderProcessGone, status="
      << std::to_underlying(status);
  guest_presentation_tracker_->Reset();
  pending_web_client_receiver_.reset();
  if (web_client_owned_) {
    UnsetWebClient(GlicWebClientLifecycleEvent::kDisconnectedOnProcessGone);
  }
  // On Android, termination of a background/hidden renderer process is reported
  // as TERMINATION_STATUS_NORMAL_TERMINATION because waitpid is not used for
  // Android service processes without a visible/strong binding. Any process
  // exit while the guest WebContents is still alive (outside browser shutdown)
  // leaves the guest unusable and must notify the delegate.
  if (!browser_shutdown::HasShutdownStarted() && delegate_) {
    delegate_->OnGuestProcessGone(status);
  }
  if (host_) {
    host_->UpdateNativeLoadingState();
  }
}

}  // namespace glic
