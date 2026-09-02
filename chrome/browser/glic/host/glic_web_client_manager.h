// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CLIENT_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CLIENT_MANAGER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/process/kill.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/glic_webui.mojom.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

namespace glic {

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

// Manages the Glic web client and observes the guest frame (WebContents).
// Owned by GlicUI.
class GlicWebClientManager : public content::WebContentsObserver {
 public:
  // Interface to the owner of GlicWebClientManager. Note, the owner will differ
  // depending on whether `kGlicNoWebview` is enabled.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // The guest contents started navigating.
    virtual void OnGuestNavigationStarted() {}
    // The guest contents finished navigating.
    virtual void OnGuestNavigated(const GURL& url,
                                  bool is_api_allowed,
                                  mojom::GuestPageType page_type,
                                  bool is_initial_commit) {}
    // The guest process was terminated.
    virtual void OnGuestProcessGone(base::TerminationStatus status) {}
    // The web client receiver was created/bound.
    virtual void OnWebClientCreated() {}
    // The web client state was changed.
    virtual void OnWebClientStateChanged(mojom::WebClientState state) {}
  };

  GlicWebClientManager();
  ~GlicWebClientManager() override;

  GlicWebClientManager(const GlicWebClientManager&) = delete;
  GlicWebClientManager& operator=(const GlicWebClientManager&) = delete;

  void SetDelegate(Delegate* delegate);

  void AttachGuestContents(content::WebContents* guest_contents);
  void AttachToHost(Host* host);

  void OnGuestNavigationBlocked(
      mojom::GuestPageType page_type = mojom::GuestPageType::kLoadError);

  content::RenderFrameHost* GetGuestMainFrame() const;
  content::WebContents* web_client_contents() const;

  GlicWebClientAccess* web_client_access() const { return web_client_; }

  void SetPendingWebClientReceiver(
      mojo::PendingReceiver<glic::mojom::WebClientHandler> web_client_receiver);
  void CreateWebClient(
      mojo::PendingReceiver<glic::mojom::WebClientHandler> web_client_receiver);
  void WebClientInitialized();
  void UnsetWebClient(
      std::optional<GlicWebClientLifecycleEvent> event = std::nullopt);

  // content::WebContentsObserver:
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override;

 private:
  void OnWebClientStateChanged(mojom::WebClientState state);

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

  mojo::PendingReceiver<glic::mojom::WebClientHandler>
      pending_web_client_receiver_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_WEB_CLIENT_MANAGER_H_
