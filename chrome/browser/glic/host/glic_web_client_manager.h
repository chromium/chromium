// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CLIENT_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CLIENT_MANAGER_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/process/kill.h"
#include "chrome/browser/glic/glic_enums.h"
#include "chrome/browser/glic/host/glic.mojom-forward.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/glic_webui.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace glic {

BASE_DECLARE_FEATURE(kGlicShowSecurityInterstitials);

class Host;

// LINT.IfChange(GlicWebClientLifecycleEvent)
enum class GlicWebClientLifecycleEvent {
  kCreated = 0,
  kInitialized = 1,
  kDisconnectedBeforeInitialization = 2,
  kDisconnectedAfterInitialization = 3,
  kDisconnectedOnNavigation = 4,
  kDisconnectedOnProcessGone = 5,
  kMaxValue = kDisconnectedOnProcessGone,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicWebClientLifecycleEvent)

// Tracks in-flight cross-document primary-main-frame navigations on an observed
// `WebContents`, updating `HasInFlightNavigation()` before subclass hooks run.
class PrimaryMainFrameNavigationTracker : public content::WebContentsObserver {
 public:
  PrimaryMainFrameNavigationTracker(const PrimaryMainFrameNavigationTracker&) =
      delete;
  PrimaryMainFrameNavigationTracker& operator=(
      const PrimaryMainFrameNavigationTracker&) = delete;

  bool HasInFlightNavigation() const { return !in_flight_ids_.empty(); }

 protected:
  PrimaryMainFrameNavigationTracker();
  ~PrimaryMainFrameNavigationTracker() override;

  void ObservePrimaryContents(content::WebContents* web_contents);

  virtual void OnPrimaryNavigationStarted(
      content::NavigationHandle* navigation_handle) {}
  virtual void OnPrimaryNavigationFinished(
      content::NavigationHandle* navigation_handle) {}
  virtual void OnPrimaryRenderProcessGone(base::TerminationStatus status) {}

 private:
  static bool IsTracked(const content::NavigationHandle* navigation_handle);

  // content::WebContentsObserver:
  void DidStartNavigation(content::NavigationHandle* navigation_handle) final;
  void DidFinishNavigation(content::NavigationHandle* navigation_handle) final;
  void PrimaryMainFrameRenderProcessGone(base::TerminationStatus status) final;

  base::flat_set<int64_t> in_flight_ids_;
};

// Manages the Glic web client and observes the guest frame (WebContents).
// Owned by GlicUI.
class GlicWebClientManager : public PrimaryMainFrameNavigationTracker {
 public:
  // Interface to the owner of GlicWebClientManager. Note, the owner will differ
  // depending on whether `GlicKeyedService::IsNoWebviewEnabled()` is true.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // The guest contents started navigating.
    virtual void OnGuestNavigationStarted() {}
    // The guest contents finished navigating, or a primary main frame
    // navigation was blocked before commit.
    virtual void OnGuestNavigated(const GURL& url,
                                  bool is_api_allowed,
                                  mojom::GuestPageType page_type,
                                  bool is_initial_commit) {}
    // The guest main frame completed its onload event for a regular allowed
    // page (when kGlicShowLoaderUntilMainPageOnload is enabled).
    virtual void OnGuestMainPageLoaded() {}
    // The guest process was terminated.
    virtual void OnGuestProcessGone(base::TerminationStatus status) {}
    // The web client receiver was created/bound.
    virtual void OnWebClientCreated() {}
    // The web client state was changed.
    virtual void OnWebClientStateChanged(mojom::WebClientState state) {}
  };

  GlicWebClientManager();
  ~GlicWebClientManager() override;

  // Maps a guest renderer `base::TerminationStatus` to the corresponding
  // `ClientLoadErrorReason` (or `mojom::ClientLoadErrorReason` wire value).
  static ClientLoadErrorReason ReasonForGuestProcessGone(
      base::TerminationStatus status);
  static mojom::ClientLoadErrorReason MojoReasonForGuestProcessGone(
      base::TerminationStatus status);

  GlicWebClientManager(const GlicWebClientManager&) = delete;
  GlicWebClientManager& operator=(const GlicWebClientManager&) = delete;

  void SetDelegate(Delegate* delegate);

  void AttachGuestContents(content::WebContents* guest_contents);
  void AttachToHost(Host* host);

  // Returns the most specific load-timeout reason matching the guest's current
  // navigation and document-load progress.
  ClientLoadErrorReason ReasonForClientLoadTimeout() const;

  // Sets whether the guest WebContents is currently shown in the panel, used
  // to know when an actionable or submittable guest is on screen.
  void SetGuestMounted(bool mounted);

  // Called when a guest navigation is cancelled before it commits. `page_type`
  // says why: kDisabledByAdmin, kSignInRequired, or kOriginBlocked.
  void OnGuestNavigationBlocked(int64_t navigation_id,
                                mojom::GuestPageType page_type,
                                const GURL& blocked_url = GURL());

  content::RenderFrameHost* GetGuestMainFrame() const;
  content::WebContents* web_client_contents() const;
  std::optional<mojom::GuestPageType> last_committed_page_type() const {
    return last_committed_page_type_;
  }
  bool guest_main_page_loaded() const { return guest_main_page_loaded_; }

  GlicWebClientAccess* web_client_access() const { return web_client_; }
  bool has_pending_web_client_receiver_for_testing() const {
    return pending_web_client_receiver_.is_valid();
  }

  void SetPendingWebClientReceiver(
      mojo::PendingReceiver<glic::mojom::WebClientHandler> web_client_receiver);
  void CreateWebClient(
      mojo::PendingReceiver<glic::mojom::WebClientHandler> web_client_receiver);
  void WebClientInitialized();
  void UnsetWebClient(
      std::optional<GlicWebClientLifecycleEvent> event = std::nullopt);

  void OnWebClientStateChangedForTesting(mojom::WebClientState state) {
    OnWebClientStateChanged(state);
  }

  // content::WebContentsObserver:
  void DocumentOnLoadCompletedInPrimaryMainFrame() override;

 private:
  class GuestPresentationTracker;
  class Metrics;

  enum class GuestLoadStage {
    kNotStarted,
    kNavigationInProgress,
    kNavigationNotCommitted,
    kCommittedNonRegularOrDisallowed,
    kCommittedRegularBeforeOnload,
    kCommittedRegularAfterOnload,
  };

  // PrimaryMainFrameNavigationTracker:
  void OnPrimaryNavigationStarted(
      content::NavigationHandle* navigation_handle) override;
  void OnPrimaryNavigationFinished(
      content::NavigationHandle* navigation_handle) override;
  void OnPrimaryRenderProcessGone(base::TerminationStatus status) override;

  void OnWebClientStateChanged(mojom::WebClientState state);
  void MaybeMarkGuestMainPageLoaded();

  std::unique_ptr<Metrics> metrics_;
  std::unique_ptr<GuestPresentationTracker> guest_presentation_tracker_;

  raw_ptr<Host> host_ = nullptr;
  raw_ptr<Delegate> delegate_ = nullptr;

  // Host owns at most one web client access. If a new access is created,
  // the old one is destroyed synchronously. This should usually not be used
  // directly, as the web client may not be initialized even if this is present.
  std::unique_ptr<GlicWebClientAccess> web_client_owned_;
  // Points to `web_client_access_` once the Javascript WebUI has completed
  // initialization and called `WebClientInitialized()`. Null before then or
  // after the web client disconnects.
  raw_ptr<GlicWebClientAccess> web_client_ = nullptr;

  // Whether the guest frame has completed at least one navigation commit.
  bool has_navigation_committed_ = false;
  // Primary-main-frame navigations that have already been handled as blocked,
  // avoiding duplicate notifications when multiple throttles or observers
  // detect the same blocked navigation.
  base::flat_set<int64_t> blocked_navigation_ids_;
  GuestLoadStage guest_load_stage_ = GuestLoadStage::kNotStarted;
  std::optional<mojom::GuestPageType> last_committed_page_type_;
  bool guest_main_page_loaded_ = false;

  mojo::PendingReceiver<glic::mojom::WebClientHandler>
      pending_web_client_receiver_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CLIENT_MANAGER_H_
