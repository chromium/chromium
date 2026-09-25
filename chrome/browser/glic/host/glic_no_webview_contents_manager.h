// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_NO_WEBVIEW_CONTENTS_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_NO_WEBVIEW_CONTENTS_MANAGER_H_

#include <memory>
#include <optional>
#include <ostream>

#include "base/callback_list.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/common/observable_value.h"
#include "chrome/browser/glic/host/glic.mojom-forward.h"
#include "chrome/browser/glic/host/glic_overlay_ui.h"
#include "chrome/browser/glic/host/glic_web_client_manager.h"
#include "chrome/browser/glic/host/glic_web_contents_manager.h"
#include "chrome/browser/glic/host/glic_zoom_controller.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_instance.h"
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

class GlicEnabling;
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
                                 public mojom::GlicOverlayPageHandler,
                                 public PanelStateObserver {
   public:
    OverlayContentsManager(Profile* profile,
                           GlicNoWebviewContentsManager* owner,
                           ObservableValueView<bool>& guest_ready);
    ~OverlayContentsManager() override;

    content::WebContents* EnsureWebContents();
    void DestroyWebContents();
    content::WebContents* web_contents() const;
    bool IsCrashed() const;
    GlicOverlayUI* GetOverlayUI() const;
    std::optional<mojom::ErrorPanelType> error_type() const;
    void SetError(mojom::ErrorPanelType error_type);
    void ClearError();
    void AttachToHost(Host* host);
    void SetVisibility(content::Visibility visibility);
    const gfx::Size& cached_size() const;
    bool ShouldReloadOnShow() const;

    // Determines what the overlay state should be based on explicit inputs:
    // - `error_type`: If an error is active, the overlay shows that error
    // panel.
    // - `is_guest_ready`: If no error is active and the guest is ready, no
    //   overlay state is needed (returns nullptr).
    // - `panel_state_kind`: Detached panels show a floating loading skeleton;
    //   otherwise, a side-panel loading skeleton is shown.
    static mojom::OverlayStatePtr DetermineOverlayState(
        std::optional<mojom::ErrorPanelType> error_type,
        bool is_guest_ready,
        std::optional<mojom::PanelStateKind> panel_state_kind);

    // Evaluates current inputs to determine the desired overlay state.
    mojom::OverlayStatePtr DetermineOverlayState();

    // Applies the computed overlay state to the overlay WebUI.
    void UpdateOverlayState();

    // PanelStateObserver implementation:
    void PanelStateChanged(const mojom::PanelState& panel_state) override;

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
    const raw_ref<ObservableValueView<bool>> guest_ready_;
    base::CallbackListSubscription guest_ready_subscription_;
    base::ScopedObservation<GlicInstance, PanelStateObserver>
        panel_state_observation_{this};
    gfx::Size cached_size_;
  };

  GlicNoWebviewContentsManager(Profile* profile,
                               GlicEnabling* enabling,
                               bool initially_hidden);
  ~GlicNoWebviewContentsManager() override;
  GlicNoWebviewContentsManager(const GlicNoWebviewContentsManager&) = delete;
  GlicNoWebviewContentsManager& operator=(const GlicNoWebviewContentsManager&) =
      delete;

  // GlicWebContentsManager implementation:
  void AttachToHost(Host* host) override;
  void SetVisibility(content::Visibility visibility) override;
  content::WebContents* active_web_contents() const override;
  content::WebContents* guest_contents() const override;
  void OnActuatingChanged(bool actuating) override;
  void OnTaskTabsVisibilityChanged(bool has_visible_tab) override;
  base::CallbackListSubscription RegisterWebContentsChangedCallback(
      WebContentsChangedCallback callback) override;
  GlicWebClientManager& web_client_manager() override;
  bool ShouldReloadOnShow() const override;
  void Zoom(mojom::ZoomAction zoom_action, ZoomSource source) override;

  // GlicWebClientManager::Delegate implementation:
  void OnGuestNavigationStarted() override;
  void OnGuestNavigated(const GURL& url,
                        bool is_api_allowed,
                        mojom::GuestPageType page_type,
                        bool is_initial_commit) override;
  void OnGuestProcessGone(base::TerminationStatus status) override;
  void OnWebClientCreated() override;
  void OnWebClientStateChanged(mojom::WebClientState state) override;

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

  // Clears any active error state from the overlay UI and updates display
  // state.
  void ClearErrorState();

  std::optional<mojom::ErrorPanelType> error_type() const {
    return overlay_manager_.error_type();
  }

  ObservableValueView<bool>& guest_ready() { return guest_ready_; }

  // Returns the Mojo page handler for the overlay UI, used for testing.
  mojom::GlicOverlayPageHandler* GetOverlayPageHandlerForTesting() const;

  mojom::OverlayStatePtr GetOverlayStateForTesting() {
    return overlay_manager_.DetermineOverlayState();
  }

  mojom::LoadingStyle GetLoadingStyleForTesting() {
    mojom::OverlayStatePtr state = GetOverlayStateForTesting();
    if (state && state->is_loading()) {
      return state->get_loading();
    }
    return mojom::LoadingStyle::kSidePanel;
  }

  OverlayContentsManager& GetOverlayManagerForTesting() {
    return overlay_manager_;
  }

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

  // Evaluates current visibility, guest readiness, and error state to determine
  // the desired display state.
  DisplayState CalculateDesiredState() const;

  // Whether the client is stuck in a state it cannot leave without being
  // reloaded. Readiness is not computed here; the Host derives that from the
  // web client connection.
  bool HasClientLoadFailed() const;

  // Reports `HasClientLoadFailed()` to the Host.
  void UpdateClientLoadFailed();

  // The state of the web client, or `kUninitialized` if no web client has
  // completed initialization.
  mojom::WebClientState web_client_state() const;

  // Re-evaluates and applies the desired display state, coordinating
  // transitions and cleaning up overlay resources.
  void UpdateDisplayState();

  // Clears transient/recoverable errors (e.g. network/offline or generic load
  // errors) if one is active, while preserving policy/auth error states (e.g.
  // sign-in required or disabled by admin).
  void ClearTransientErrorState();

  // Applies the cached viewport size from the overlay/host to the guest view.
  void ApplySizeToGuest();

  // Transitions the manager to `next_state`, coordinating overlay allocation,
  // destruction, and active WebContents change notifications.
  void TransitionTo(DisplayState next_state);

  // Updates the performance traits tracker with actuation state changes.
  void UpdateActuationTracker();

  void OnZoomLevelChange();
  void OnProfileReadyStateChanged();
  void UpdateForProfileReadyState(bool is_initial);

  raw_ptr<Profile> profile_;
  raw_ptr<GlicEnabling> enabling_ = nullptr;
  raw_ptr<Host> host_ = nullptr;

  // True if the guest WebContents is the currently active view presented by
  // `active_web_contents()`.
  ObservableValue<bool> guest_ready_{false};

  GlicWebClientManager web_client_manager_;
  OverlayContentsManager overlay_manager_;
  std::unique_ptr<pwc::PrivilegedWebContents> privileged_guest_contents_;
  GlicZoomController zoom_controller_;

  // Current display lifecycle state.
  DisplayState state_ = DisplayState::kWarming;

  // True if Glic is currently visible to the user.
  bool is_visible_ = false;

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

  base::CallbackListSubscription profile_ready_subscription_;

  base::OneShotTimer overlay_deletion_timer_;

  base::WeakPtrFactory<GlicNoWebviewContentsManager> weak_ptr_factory_{this};
};

inline std::ostream& operator<<(
    std::ostream& os,
    GlicNoWebviewContentsManager::DisplayState state) {
  switch (state) {
    case GlicNoWebviewContentsManager::DisplayState::kWarming:
      return os << "kWarming";
    case GlicNoWebviewContentsManager::DisplayState::kAttachedHidden:
      return os << "kAttachedHidden";
    case GlicNoWebviewContentsManager::DisplayState::kShowingOverlay:
      return os << "kShowingOverlay";
    case GlicNoWebviewContentsManager::DisplayState::kShowingGuest:
      return os << "kShowingGuest";
  }
}

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_NO_WEBVIEW_CONTENTS_MANAGER_H_
