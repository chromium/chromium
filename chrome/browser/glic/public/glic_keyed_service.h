// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_GLIC_KEYED_SERVICE_H_
#define CHROME_BROWSER_GLIC_PUBLIC_GLIC_KEYED_SERVICE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_set.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/actor/actor_task_delegate.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_zero_state_suggestions_manager.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_provider.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/actor_webui.mojom-forward.h"
#include "components/autofill/core/browser/integrators/glic/actor_form_filling_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

class BrowserWindowInterface;
class Profile;
class ProfileManager;

namespace actor {
class ActorKeyedService;
}  // namespace actor

namespace contextual_cueing {
class ContextualCueingService;
}  // namespace contextual_cueing

namespace signin {
class IdentityManager;
}  // namespace signin

namespace glic {

class AuthController;
class GlicEnabling;
class GlicFreController;
class GlicMetrics;
class GlicOcclusionNotifier;
class GlicProfileManager;
class GlicRegionCaptureController;
class GlicScreenshotCapturer;
class GlicShareImageHandler;
class GlicTabDataObserver;
class GlicWindowController;
class HostManager;
class GlicActorTaskManager;

enum class GlicPrewarmingChecksResult;

// LINT.IfChange(GlicPrewarmingFreSource)
enum class GlicPrewarmingFreSource {
  kWhatsNew = 0,
  kNudge = 1,
  kIph = 2,
  kTest = 3,
  kBrowserCommand = 4,
  kMaxValue = kBrowserCommand,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicPrewarmingFreSource)

// The GlicKeyedService is created for each eligible (i.e. non-incognito,
// non-system, etc.) browser profile if Glic flags are enabled, regardless
// of whether the profile is enabled or disabled at runtime (currently
// possible via enterprise policy). This is required on disabled profiles
// since pieces of this service are the ones that monitor this runtime
// preference for changes and cause the UI to respond to it.
class GlicKeyedService : public KeyedService,
                         public GlicSharingManagerProvider,
                         public Host::InstanceDelegate,
                         public base::MemoryPressureListener,
                         public actor::ActorTaskDelegate {
 public:
  explicit GlicKeyedService(
      Profile* profile,
      signin::IdentityManager* identity_manager,
      ProfileManager* profile_manager,
      GlicProfileManager* glic_profile_manager,
      contextual_cueing::ContextualCueingService* contextual_cueing_service,
      actor::ActorKeyedService* actor_keyed_service);
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
  // TODO(b:448888544): remove `prevent_close` in favor of a Show method.
  void ToggleUI(BrowserWindowInterface* bwi,
                bool prevent_close,
                mojom::InvocationSource source,
                std::optional<std::string> prompt_suggestion = std::nullopt);

  void OpenFreDialogInNewTab(BrowserWindowInterface* bwi,
                             mojom::InvocationSource source);

  // Forcibly close the UI. This is similar to Shutdown in that it causes the
  // window controller to shutdown (and clear cached state), but unlike
  // Shutdown, it doesn't unregister as the "active glic" with the profile
  // manager.
  // TODO(crbug.com/454112198): Remove when multi-instance launches.
  void CloseAndShutdown();

  // Close the active embedder and clear contents for an instance associated
  // with this render frame host.
  void CloseAndShutdown(content::RenderFrameHost* render_frame_host);

  // Close the panel. Virtual for testing.
  // TODO(crbug.com/448406730): Remove testing logic that relies on
  // GKS::CloseFloatingPanel since close panel is now being handled by
  // EmbedderDelegate.
  virtual void CloseFloatingPanel();

  GlicEnabling& enabling() { return *enabling_.get(); }

  GlicMetrics* metrics() { return metrics_.get(); }
  GlicFreController& fre_controller();
  GlicWindowController& window_controller() const;
  GlicWindowControllerInterface& GetSingleInstanceWindowController() const;
  GlicSharingManager& sharing_manager() override;

  // Called when a webview guest is created within a chrome://glic WebUI.
  void GuestAdded(content::WebContents* guest_contents);

  // Virtual for testing.
  virtual bool IsWindowShowing() const;

  // Returns true if `bwi` has a glic panel showing for its active tab.
  bool IsPanelShowingForBrowser(const BrowserWindowInterface& bwi) const;

  // Virtual for testing.
  virtual bool IsWindowDetached() const;

  bool IsWindowOrFreShowing() const;

  // Private API for the glic WebUI.

  void SetContextAccessIndicator(bool show);

  // Callback for changes to the context access indicator status.
  using ContextAccessIndicatorChangedCallback =
      base::RepeatingCallback<void(bool)>;

  // Registers a callback to be called any time the context access indicator
  // status changes. This is used to update UI effects on the focused tab
  // depending on whether the client has requested the indicators or not.
  base::CallbackListSubscription AddContextAccessIndicatorStatusChangedCallback(
      ContextAccessIndicatorChangedCallback callback);

  // Returns whether the context access indicator should be shown for the web
  // contents. True iff the web contents is considered focused by
  // GlicFocusedTabManager and the web client has enabled the context access
  // indicator.
  bool IsContextAccessIndicatorShown(const content::WebContents* contents);

  bool is_context_access_indicator_enabled() const {
    return is_context_access_indicator_enabled_;
  }

  // Host::InstanceDelegate:
  // CreateTab is used by both the FRE page and the glic web client to open a
  // URL in a new tab. The source is the RenderFrameHost of the Glic
  // instance that is requesting the navigation - this gets set as the
  // navigation handle's opener param.
  tabs::TabInterface* CreateTab(
      const ::GURL& url,
      bool open_in_background,
      const std::optional<int32_t>& window_id,
      glic::mojom::WebClientHandler::CreateTabCallback callback) override;
  void CreateTask(
      base::WeakPtr<actor::ActorTaskDelegate> delegate,
      actor::webui::mojom::TaskOptionsPtr options,
      mojom::WebClientHandler::CreateTaskCallback callback) override;
  void PerformActions(
      const std::vector<uint8_t>& actions_proto,
      mojom::WebClientHandler::PerformActionsCallback callback) override;
  void StopActorTask(actor::TaskId task_id,
                     mojom::ActorTaskStopReason stop_reason) override;
  void PauseActorTask(actor::TaskId task_id,
                      mojom::ActorTaskPauseReason pause_reason,
                      tabs::TabInterface::Handle tab_handle) override;
  // TODO(crbug.com/446696379) - The ResumeActorTask Glic API should, like the
  // rest of actor observations, operate in terms of TabObservation rather than
  // TabContext.
  void ResumeActorTask(
      actor::TaskId task_id,
      const mojom::GetTabContextOptions& context_options,
      glic::mojom::WebClientHandler::ResumeActorTaskCallback callback) override;
  void InterruptActorTask(actor::TaskId task_id) override;
  void UninterruptActorTask(actor::TaskId task_id) override;
  void CreateActorTab(
      actor::TaskId task_id,
      bool open_in_background,
      const std::optional<int32_t>& initiator_tab_id,
      const std::optional<int32_t>& initiator_window_id,
      glic::mojom::WebClientHandler::CreateActorTabCallback callback) override;
  void FetchZeroStateSuggestions(
      bool is_first_run,
      std::optional<std::vector<std::string>> supported_tools,
      glic::mojom::WebClientHandler::
          GetZeroStateSuggestionsForFocusedTabCallback callback) override;
  void GetZeroStateSuggestionsAndSubscribe(
      bool has_active_subscription,
      const mojom::ZeroStateSuggestionsOptions& options,
      mojom::WebClientHandler::GetZeroStateSuggestionsAndSubscribeCallback
          callback) override;
  void RegisterConversation(
      glic::mojom::ConversationInfoPtr info,
      mojom::WebClientHandler::RegisterConversationCallback callback) override;
  void OnWebClientCleared() override;
  void PrepareForOpen() override;
  void OnInteractionModeChange(mojom::WebClientMode new_mode) override;
  glic::GlicInstanceMetrics* instance_metrics() override;
  bool IsActive() override;

  void OnUserInputSubmitted(glic::mojom::WebClientMode mode);

  // Registers a callback to be called any time user input is submitted in the
  // client. This is used to update UI effects on tabs that are being shared
  // with glic.
  base::CallbackListSubscription AddUserInputSubmittedCallback(
      base::RepeatingClosure callback);

  void CaptureRegion(
      content::WebContents* web_contents,
      mojo::PendingRemote<mojom::CaptureRegionObserver> observer);

  // Fetches the image for the context menu item (if possible, and potentially
  // scaling and reencoding) and sends the result to the web client as
  // additional data.
  void ShareContextImage(tabs::TabInterface* tab,
                         content::RenderFrameHost* frame,
                         const ::GURL& src_url);

  AuthController& GetAuthController() { return *auth_controller_; }

  GlicRegionCaptureController& region_capture_controller();

  bool IsActiveWebContents(content::WebContents* contents);

  void AddPreloadCallback(base::OnceCallback<void()> callback);

  virtual void TryPreload();
  void TryPreloadAfterDelay();
  virtual void TryPreloadFre(GlicPrewarmingFreSource source);
  void Reload(content::RenderFrameHost* render_frame_host);
  // Close the active embedder for an instance associated with this render frame
  // host.
  void Close(content::RenderFrameHost* outermost_render_frame_host);
  Profile* profile() const { return profile_; }

  // Used only for testing purposes.
  void reset_profile_for_test() { profile_ = nullptr; }

  base::WeakPtr<GlicKeyedService> GetWeakPtr();

  void OnMemoryPressure(base::MemoryPressureLevel level) override;

  // ActorTaskDelegate:
  void OnTabAddedToTask(actor::TaskId task_id,
                        const tabs::TabInterface::Handle& tab_handle) override;

  HostManager& host_manager();

  // Null in multi-instance mode.
  GlicZeroStateSuggestionsManager* zero_state_suggestions_manager() {
    return zero_state_suggestions_manager_.get();
  }

  // Returns whether this process host is either the Glic FRE WebUI or the Glic
  // main WebUI.
  bool IsProcessHostForGlic(content::RenderProcessHost* process_host);
  // Returns whether this web contents contains the Chrome glic WebUI,
  // chrome://glic.
  bool IsGlicWebUi(content::WebContents* web_contents);

  // Get the GlicInstance associated with the given browser's active tab, or
  // null if there is none. `bwi` can be null if preloaded with no browser open.
  GlicInstance* GetInstanceForActiveTab(BrowserWindowInterface* bwi);

  // Get the GlicInstance for a provided tab, or null if there is none.
  GlicInstance* GetInstanceForTab(tabs::TabInterface* tab);

  // Sends additional context to the web client associated with the given tab.
  // If no web client exists for the tab, then this method does nothing. It is
  // the responsibility of the caller to ensure that a host exists before
  // calling this method.
  void SendAdditionalContext(tabs::TabHandle tab_handle,
                             mojom::AdditionalContextPtr context);

  // Registers a callback to be invoked when the TabData for an explicitly
  // observed tab changes. Note that currently, only tabs observed via
  // `OnTabAddedToTask` trigger updates.
  using TabDataChangedCallback =
      base::RepeatingCallback<void(const TabDataChange&)>;
  base::CallbackListSubscription AddTabDataChangedCallback(
      TabDataChangedCallback callback);

  // ActorTaskDelegate:
  void RequestToShowCredentialSelectionDialog(
      actor::TaskId task_id,
      const base::flat_map<std::string, gfx::Image>& icons,
      const std::vector<actor_login::Credential>& credentials,
      actor::ActorTaskDelegate::CredentialSelectedCallback callback) override;
  void RequestToShowUserConfirmationDialog(
      actor::TaskId task_id,
      const url::Origin& navigation_origin,
      bool for_blocklisted_origin,
      actor::ActorTaskDelegate::UserConfirmationDialogCallback callback)
      override;
  void RequestToConfirmNavigation(
      actor::TaskId task_id,
      const url::Origin& navigation_origin,
      actor::ActorTaskDelegate::NavigationConfirmationCallback callback)
      override;
  void RequestToShowAutofillSuggestionsDialog(
      actor::TaskId task_id,
      std::vector<autofill::ActorFormFillingRequest> requests,
      AutofillSuggestionSelectedCallback callback) override;

 private:
  // A helper function to route GetZeroStateSuggestionsForFocusedTabCallback
  // callbacks.
  void OnZeroStateSuggestionsFetched(
      glic::mojom::ZeroStateSuggestionsPtr suggestions,
      glic::mojom::WebClientHandler::
          GetZeroStateSuggestionsForFocusedTabCallback callback,
      std::vector<std::string> returned_suggestions);

  void FinishPreload(GlicPrewarmingChecksResult reason);
  void FinishPreloadFre(GlicPrewarmingFreSource source,
                        GlicPrewarmingChecksResult result);

  // List of callbacks to be notified when the client requests a change to the
  // context access indicator status.
  base::RepeatingCallbackList<void(bool)>
      context_access_indicator_callback_list_;
  // The state of the context access indicator as set by the client.
  bool is_context_access_indicator_enabled_ = false;

  // List of callbacks to be notified when user input has been submitted.
  base::RepeatingClosureList user_input_submitted_callback_list_;

  raw_ptr<Profile> profile_;

  std::unique_ptr<GlicEnabling> enabling_;
  std::unique_ptr<GlicMetrics> metrics_;
  std::unique_ptr<GlicFreController> fre_controller_;
  // Is either a GlicWindowControllerImpl or GlicPanelCoordinatorImpl.
  std::unique_ptr<GlicWindowController> window_controller_;
  std::unique_ptr<GlicSharingManager> sharing_manager_;
  std::unique_ptr<GlicShareImageHandler> share_image_handler_;
  std::unique_ptr<GlicScreenshotCapturer> screenshot_capturer_;
  std::unique_ptr<GlicRegionCaptureController> region_capture_controller_;
  std::unique_ptr<AuthController> auth_controller_;
  std::unique_ptr<base::MemoryPressureListenerRegistration>
      memory_pressure_listener_registration_;
  // Null in multi-instance mode.
  std::unique_ptr<GlicOcclusionNotifier> occlusion_notifier_;
  std::unique_ptr<GlicZeroStateSuggestionsManager>
      zero_state_suggestions_manager_;
  base::OnceCallback<void()> preload_callback_;
  std::unique_ptr<GlicActorTaskManager> actor_task_manager_;
  std::unique_ptr<GlicTabDataObserver> tab_data_observer_;

  // Unowned
  raw_ptr<contextual_cueing::ContextualCueingService>
      contextual_cueing_service_;

  base::WeakPtrFactory<GlicKeyedService> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_KEYED_SERVICE_H_
