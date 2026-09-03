// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_NO_WEBVIEW_CONTENTS_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_NO_WEBVIEW_CONTENTS_MANAGER_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_overlay_ui.h"
#include "chrome/browser/glic/host/glic_web_client_manager.h"
#include "chrome/browser/glic/host/glic_web_contents_manager.h"
#include "chrome/browser/glic/host/host.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/gfx/geometry/size.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace pwc {
class PrivilegedWebContents;
}

namespace glic {

class Host;

// Manages the WebContents instances for Glic.
//
// Hosts the guest web page in a top-level WebContents wrapped in a
// `PrivilegedWebContents`. An on-demand WebUI overlay WebContents
// (`chrome://glic/overlay`) displays initial loading animations and error
// screens (such as sign-in prompts, administrator policy restrictions, or guest
// errors) while the guest is unready, and is destroyed once the guest client
// successfully connects.
//
// Implements `GlicWebContentsManager` to provide the active WebContents to the
// host UI, swapping from the overlay WebContents to the guest WebContents once
// the guest is connected and responsive.
//
// TODO(b/555365681): Rename to GlicWebContentsManagerImpl once
// GlicWebUIContentsManager is removed.
class GlicNoWebviewContentsManager : public GlicWebContentsManager,
                                     public GlicWebClientManager::Delegate {
 public:
  // Manages the lifetime, WebUI page handler bindings, and observer events
  // for the loading/error overlay WebContents (chrome://glic/overlay).
  class OverlayContentsManager : public content::WebContentsObserver,
                                 public mojom::GlicOverlayPageHandler {
   public:
    OverlayContentsManager(Profile* profile,
                           GlicNoWebviewContentsManager* owner);
    ~OverlayContentsManager() override;

    content::WebContents* EnsureWebContents();
    void DestroyWebContents();
    content::WebContents* web_contents() const;
    bool IsCrashed() const;
    GlicOverlayUI* GetOverlayUI() const;
    std::optional<mojom::ErrorPanelType> error_type() const;
    void SetError(mojom::ErrorPanelType error_type);
    void SetVisibility(content::Visibility visibility);
    const gfx::Size& cached_size() const;
    bool ShouldReloadOnShow() const;

    mojom::GlicOverlayPageHandler* GetPageHandlerForTesting() { return this; }

   private:
    // content::WebContentsObserver:
    void RenderFrameCreated(
        content::RenderFrameHost* render_frame_host) override;
    void DidFinishNavigation(
        content::NavigationHandle* navigation_handle) override;
    void PrimaryMainFrameWasResized(bool width_changed) override;

    // mojom::GlicOverlayPageHandler:
    void OnRetryClicked() override;
    void OnSignInClicked() override;
    void OnProfilePickerClicked() override;
    void OnIneligibleAccountHelpClicked() override;
    void OnLocationMismatchHelpClicked() override;
    void OnDisabledByAdminCloseClicked() override;
    void OnDisabledByAdminLinkClicked() override;
    void OnClosePanelClicked() override;

    void OpenUrlAndClosePanel(const GURL& url);

    raw_ptr<Profile> profile_;
    raw_ptr<GlicNoWebviewContentsManager> owner_;
    std::unique_ptr<content::WebContents> web_contents_;
    std::optional<mojom::ErrorPanelType> error_type_;
    gfx::Size cached_size_;
  };

  GlicNoWebviewContentsManager(Profile* profile, bool initially_hidden);
  ~GlicNoWebviewContentsManager() override;
  GlicNoWebviewContentsManager(const GlicNoWebviewContentsManager&) = delete;
  GlicNoWebviewContentsManager& operator=(const GlicNoWebviewContentsManager&) =
      delete;

  // GlicWebContentsManager implementation:
  void AttachToHost(Host* host) override;
  void SetVisibility(content::Visibility visibility) override;
  content::WebContents* active_web_contents() const override;
  void OnActuatingChanged(bool actuating) override;
  void OnTaskTabsVisibilityChanged(bool has_visible_tab) override;
  std::unique_ptr<content::WebContents> ReleaseWebContents() override;
  void ReclaimWebContents(
      std::unique_ptr<content::WebContents> web_contents) override;
  base::CallbackListSubscription RegisterWebContentsChangedCallback(
      WebContentsChangedCallback callback) override;
  GlicWebClientManager& web_client_manager() override;
  bool ShouldReloadOnShow() const override;

  // GlicWebClientManager::Delegate implementation:
  void OnGuestNavigationStarted() override;
  void OnGuestNavigated(const GURL& url,
                        bool is_api_allowed,
                        mojom::GuestPageType page_type,
                        bool is_initial_commit) override;
  void OnGuestProcessGone(base::TerminationStatus status) override;
  void OnWebClientCreated() override;
  void OnWebClientStateChanged(mojom::WebClientState state) override;

  // Returns the guest WebContents, or nullptr if destroyed.
  content::WebContents* guest_contents() const;

  // Returns the loading/error overlay WebContents if currently allocated.
  content::WebContents* overlay_contents() const;

  // Lifecycle display states of the container.
  enum class DisplayState {
    // In warming pool, guest loading in background, overlay not created.
    kWarming,
    // Attached to Host, but hidden; overlay not created.
    kAttachedHidden,
    // Visible to user, guest not yet ready; overlay is created and showing.
    kShowingOverlay,
    // Guest connected and active; overlay is destroyed.
    kShowingGuest,
  };

  DisplayState state() const { return state_; }

  Profile* profile() const { return profile_; }

  // Transitions the manager into an error state, showing the corresponding
  // error panel on the overlay UI. If warming in the background or hidden,
  // the error is recorded without creating the overlay WebContents until shown.
  void SetErrorState(mojom::ErrorPanelType error_type);

  // Returns the Mojo page handler for the overlay UI, used for testing.
  mojom::GlicOverlayPageHandler* GetOverlayPageHandlerForTesting() const;

  const base::OneShotTimer& overlay_deletion_timer_for_testing() const {
    return overlay_deletion_timer_;
  }

 private:
  // Ensures that the overlay WebContents exists and returns it.
  content::WebContents* EnsureOverlayContents();

  // Destroys the overlay WebContents to reclaim memory once the guest is ready.
  void DestroyOverlayContents();

  // Schedules deletion of the overlay WebContents after `delay`. If a deletion
  // is already pending, this replaces/reschedules it. Shortcuts if no overlay
  // exists.
  void ScheduleOverlayDeletion(base::TimeDelta delay);

  // Cancels any scheduled overlay deletion.
  void CancelOverlayDeletion();

  // Initiates navigation of the guest WebContents to the Glic guest URL.
  void LoadGuest();

  // Notifies registered subscribers that the active WebContents has changed
  // (e.g. when swapping between overlay and guest).
  void NotifyWebContentsChanged();

  // Injects the bootstrap ping script into the guest main frame to initiate the
  // Glic API client connection.
  void StartGuestBootstrap();

  // Cancels the in-guest bootstrap ping interval.
  void StopGuestBootstrap();

  // Swaps the active WebContents presented to the host view to the guest once
  // the client is connected and ready.
  void MaybeSwapToGuest();

  // Applies the cached viewport size from the overlay/host to the guest view.
  void ApplySizeToGuest();

  // Transitions the manager to `next_state`, coordinating overlay allocation,
  // destruction, and active WebContents change notifications.
  void TransitionTo(DisplayState next_state);

  // Updates the performance traits tracker with actuation state changes.
  void UpdateActuationTracker();

  raw_ptr<Profile> profile_;
  raw_ptr<Host> host_ = nullptr;

  GlicWebClientManager web_client_manager_;
  OverlayContentsManager overlay_manager_;
  std::unique_ptr<pwc::PrivilegedWebContents> privileged_guest_contents_;

  // Current display lifecycle state.
  DisplayState state_ = DisplayState::kWarming;

  // True if Glic is currently visible to the user.
  bool is_visible_ = false;

  // True if the guest WebContents is the currently active view presented by
  // `active_web_contents()`.
  bool is_guest_ready_ = false;

  // True if the guest navigated to an error page (e.g. /sorry/).
  bool is_guest_error_ = false;

  // Actuation tracking state.
  bool is_actuating_ = false;
  bool is_actuating_on_visible_tab_ = false;
  base::ScopedClosureRunner guest_capture_runner_;

  // Cached viewport size from the overlay used to size the guest before swap.
  gfx::Size cached_overlay_size_;

  base::RepeatingCallbackList<void(content::WebContents*)>
      web_contents_changed_callbacks_;

  base::OneShotTimer overlay_deletion_timer_;

  base::WeakPtrFactory<GlicNoWebviewContentsManager> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_NO_WEBVIEW_CONTENTS_MANAGER_H_
