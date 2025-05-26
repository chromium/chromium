// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_H_
#define CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/glic/host/context/glic_focused_tab_manager.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/web_contents.h"

class BrowserWindowInterface;
class Profile;
class ProfileManager;

namespace actor {
class ActorCoordinator;
}  // namespace actor

namespace contextual_cueing {
class ContextualCueingService;
}  // namespace contextual_cueing

namespace signin {
class IdentityManager;
}  // namespace signin

namespace glic {
class AuthController;
class GlicActorController;
class FocusedTabData;
class GlicEnabling;
class GlicMetrics;
class GlicProfileManager;
class GlicScreenshotCapturer;
class GlicWindowControllerImpl;

// The GlicKeyedService is created for each eligible (i.e. non-incognito,
// non-system, etc.) browser profile if Glic flags are enabled, regardless
// of whether the profile is enabled or disabled at runtime (currently
// possible via enterprise policy). This is required on disabled profiles
// since pieces of this service are the ones that monitor this runtime
// preference for changes and cause the UI to respond to it.
class GlicKeyedService : public KeyedService {
 public:
  explicit GlicKeyedService(
      Profile* profile,
      signin::IdentityManager* identity_manager,
      ProfileManager* profile_manager,
      GlicProfileManager* glic_profile_manager,
      contextual_cueing::ContextualCueingService* contextual_cueing_service);
  GlicKeyedService(const GlicKeyedService&) = delete;
  GlicKeyedService& operator=(const GlicKeyedService&) = delete;
  ~GlicKeyedService() override;

  // Convenience method, may return nullptr.
  static GlicKeyedService* Get(content::BrowserContext* context);

  // KeyedService
  void Shutdown() override;

  // Show, summon or activate the panel, or close it if it's already active and
  // prevent_close is false. If glic_button_view is non-null, attach the panel
  // to that view's Browser.
  void ToggleUI(BrowserWindowInterface* bwi,
                bool prevent_close,
                mojom::InvocationSource source);

  void OpenFreDialogInNewTab(BrowserWindowInterface* bwi);

  // Forcibly close the UI. This is similar to Shutdown in that it causes the
  // window controller to shutdown (and clear cached state), but unlike
  // Shutdown, it doesn't unregister as the "active glic" with the profile
  // manager.
  void CloseUI();

  // The user has performed an action suggesting that they made open the UI
  // soon.
  void PrepareForOpen();

  // Fetch zero state suggestions for the active web contents.
  void FetchZeroStateSuggestions(
      bool is_first_run,
      glic::mojom::WebClientHandler::
          GetZeroStateSuggestionsForFocusedTabCallback callback);

  GlicEnabling& enabling() { return *enabling_.get(); }

  GlicMetrics* metrics() { return metrics_.get(); }
  GlicWindowController& window_controller();

  // Called when a webview guest is created within a chrome://glic WebUI.
  void GuestAdded(content::WebContents* guest_contents);

  // Virtual for testing.
  virtual bool IsWindowShowing() const;

  // Virtual for testing.
  virtual bool IsWindowDetached() const;

  // Private API for the glic WebUI.

  // CreateTab is used by both the FRE page and the glic web client to open a
  // URL in a new tab.
  void CreateTab(const ::GURL& url,
                 bool open_in_background,
                 const std::optional<int32_t>& window_id,
                 glic::mojom::WebClientHandler::CreateTabCallback callback);
  virtual void ClosePanel();
  void AttachPanel();
  void DetachPanel();
  void ResizePanel(const gfx::Size& size,
                   base::TimeDelta duration,
                   base::OnceClosure callback);
  void SetPanelDraggableAreas(const std::vector<gfx::Rect>& draggable_areas);
  void SetContextAccessIndicator(bool show);

  // Callback for all changes to focused tab.
  using FocusedTabChangedCallback =
      base::RepeatingCallback<void(FocusedTabData)>;
  // Callback for changes to focused tab data.
  using FocusedTabDataChangedCallback =
      base::RepeatingCallback<void(const glic::mojom::TabData*)>;
  // Callback for changes to the focused tab instance.
  using FocusedTabInstanceChangedCallback =
      base::RepeatingCallback<void(content::WebContents*)>;
  // Callback for changes to the focused tab container or candidate instances.
  using FocusedTabOrCandidateInstanceChangedCallback =
      base::RepeatingCallback<void(FocusedTabData)>;
  // Callback for changes to the context access indicator status.
  using ContextAccessIndicatorChangedCallback =
      base::RepeatingCallback<void(bool)>;

  // Registers |callback| to be called whenever the focused tab changes. This
  // includes when the active/selected tab for the profile changes (including
  // those resulting from browser/window changes) as well as when the primary
  // page of the focused tab has changed internally. Subscribers can filter for
  // specific subsets of changes they care about by holding on to their own
  // internal state. When the focused tab changes to nothing, this will be
  // called with nullptr as the supplied WebContents argument.
  base::CallbackListSubscription AddFocusedTabChangedCallback(
      FocusedTabChangedCallback callback);

  // Callback for changes to either the focused tab or the focused tab candidate
  // instances. If no tab is in focus an error reason is returned indicating
  // why and maybe a tab candidate with details as to why it cannot be focused.
  base::CallbackListSubscription
  AddFocusedTabOrCandidateInstanceChangedCallback(
      FocusedTabOrCandidateInstanceChangedCallback callback);

  // Callback for changes to the `WebContents` comprising the focused tab. Only
  // fired when the `WebContents` for the focused tab changes to/from nullptr or
  // to different `WebContents` instance.
  base::CallbackListSubscription AddFocusedTabInstanceChangedCallback(
      FocusedTabInstanceChangedCallback callback);

  // Callback for changes to the tab data representation of the focused tab.
  // This includes any event that changes tab data -- e.g. favicon/title change
  // events (where the container does not change), as well as container changed
  // events.
  base::CallbackListSubscription AddFocusedTabDataChangedCallback(
      FocusedTabDataChangedCallback callback);

  // Registers a callback to be called any time the context access indicator
  // status changes. This is used to update UI effects on the focused tab
  // depending on whether the client has requested the indicators or not.
  base::CallbackListSubscription AddContextAccessIndicatorStatusChangedCallback(
      ContextAccessIndicatorChangedCallback callback);

  // Returns the currently focused tab data union which contains the focused
  // tab's web contents, or the candidate for the focused tab and why it was
  // deemed invalid for focus, or an error stating why no candidate was
  // available.
  FocusedTabData GetFocusedTabData();

  // Returns whether the context access indicator should be shown for the web
  // contents. True iff the web contents is considered focused by
  // GlicFocusedTabManager and the web client has enabled the context access
  // indicator.
  bool IsContextAccessIndicatorShown(const content::WebContents* contents);

  bool is_context_access_indicator_enabled() const {
    return is_context_access_indicator_enabled_;
  }

  void GetContextFromFocusedTab(
      const mojom::GetTabContextOptions& options,
      glic::mojom::WebClientHandler::GetContextFromFocusedTabCallback callback);

  void ActInFocusedTab(
      const std::vector<uint8_t>& action_proto,
      const mojom::GetTabContextOptions& options,
      mojom::WebClientHandler::ActInFocusedTabCallback callback);

  void StopActorTask(actor::TaskId task_id);
  void PauseActorTask(actor::TaskId task_id);
  void ResumeActorTask(
      actor::TaskId task_id,
      const mojom::GetTabContextOptions& context_options,
      glic::mojom::WebClientHandler::ResumeActorTaskCallback callback);

  // Returns true if the associated ActorCoordinator is active on the given
  // `tab`. This can be used by callers to customize certain behaviour that
  // might interfere with the ActorCoordinator.
  bool IsActorCoordinatorActingOnTab(const content::WebContents* tab) const;

  actor::ActorCoordinator& GetActorCoordinatorForTesting();

  void CaptureScreenshot(
      glic::mojom::WebClientHandler::CaptureScreenshotCallback callback);

  AuthController& GetAuthController() { return *auth_controller_; }

  bool IsActiveWebContents(content::WebContents* contents);

  virtual void TryPreload();
  virtual void TryPreloadFre();
  void Reload();

  Profile* profile() const { return profile_; }

  base::WeakPtr<GlicKeyedService> GetWeakPtr();

  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel level);

  Host& host() { return *host_; }
  // Returns whether this process host is either the Glic FRE WebUI or the Glic
  // main WebUI.
  bool IsProcessHostForGlic(content::RenderProcessHost* process_host);
  // Returns whether this web contents contains the Chrome glic WebUI,
  // chrome://glic.
  bool IsGlicWebUi(content::WebContents* web_contents);

 private:
  // A helper function to route GetZeroStateSuggestionsForFocusedTabCallback
  // callbacks.
  void OnZeroStateSuggestionsFetched(
      glic::mojom::ZeroStateSuggestionsPtr suggestions,
      glic::mojom::WebClientHandler::
          GetZeroStateSuggestionsForFocusedTabCallback callback,
      std::optional<std::vector<std::string>> returned_suggestions);

  void FinishPreload(Profile* profile, bool should_preload);
  void FinishPreloadFre(Profile* profile, bool should_preload);

  // List of callbacks to be notified when the client requests a change to the
  // context access indicator status.
  base::RepeatingCallbackList<void(bool)>
      context_access_indicator_callback_list_;
  // The state of the context access indicator as set by the client.
  bool is_context_access_indicator_enabled_ = false;

  raw_ptr<Profile> profile_;

  std::unique_ptr<GlicEnabling> enabling_;
  std::unique_ptr<GlicMetrics> metrics_;
  std::unique_ptr<Host> host_;
  std::unique_ptr<GlicWindowControllerImpl> window_controller_;
  GlicFocusedTabManager focused_tab_manager_;
  std::unique_ptr<GlicScreenshotCapturer> screenshot_capturer_;
  std::unique_ptr<AuthController> auth_controller_;
  std::unique_ptr<GlicActorController> actor_controller_;
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;

  // Unowned
  raw_ptr<contextual_cueing::ContextualCueingService>
      contextual_cueing_service_;

  base::WeakPtrFactory<GlicKeyedService> weak_ptr_factory_{this};
};

}  // namespace glic
#endif  // CHROME_BROWSER_GLIC_GLIC_KEYED_SERVICE_H_
