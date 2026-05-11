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
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/glic/common/local_hotkey_manager.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/actor_webui.mojom-forward.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

class BrowserWindowInterface;
class Profile;
class ProfileManager;

namespace actor {
class ActorKeyedService;
}  // namespace actor

namespace signin {
class IdentityManager;
}  // namespace signin

namespace glic {

class AuthController;
class ContextualCueingService;
class GlicActorPolicyChecker;
class GlicEnabling;
class GlicFreController;
class GlicMetrics;
class GlicProfileManager;
class GlicShareImageHandler;
class GlicTabDataObserver;
class GlicTabFaviconObserver;
class GlicInstanceCoordinator;

#if !BUILDFLAG(IS_ANDROID)
class GlicExperimentalOptInController;
#endif

enum class GlicPrewarmingChecksResult;

// The GlicKeyedService is created for each eligible (i.e. non-incognito,
// non-system, etc.) browser profile if Glic flags are enabled, regardless
// of whether the profile is enabled or disabled at runtime (currently
// possible via enterprise policy). This is required on disabled profiles
// since pieces of this service are the ones that monitor this runtime
// preference for changes and cause the UI to respond to it.
class GlicKeyedService : public KeyedService, public base::SupportsUserData {
 public:
  explicit GlicKeyedService(
      Profile* profile,
      signin::IdentityManager* identity_manager,
      ProfileManager* profile_manager,
      GlicProfileManager* glic_profile_manager,
      glic::ContextualCueingService* contextual_cueing_service,
      actor::ActorKeyedService* actor_keyed_service);
  GlicKeyedService(const GlicKeyedService&) = delete;
  GlicKeyedService& operator=(const GlicKeyedService&) = delete;
  ~GlicKeyedService() override;

#if BUILDFLAG(IS_ANDROID)
  // Returns a Java object of the type GlicKeyedService for the given
  // GlicKeyedService.
  static base::android::ScopedJavaLocalRef<jobject> GetJavaObject(
      GlicKeyedService* glic_keyed_service);
#endif  // BUILDFLAG(IS_ANDROID)

  // Convenience method, may return nullptr.
  static GlicKeyedService* Get(content::BrowserContext* context);

  // KeyedService
  void Shutdown() override;

  // Show, summon or activate the panel, or close it if it's already active and
  // prevent_close is false. If `bwi` is non-null, attach the panel to its
  // Browser.
  // TODO(b:448888544): remove `prevent_close` in favor of a Show method.

  virtual void ToggleUI(BrowserWindowInterface* bwi,
                        bool prevent_close,
                        mojom::InvocationSource source,
                        std::optional<std::string> prompt_suggestion);
  virtual void ToggleUI(BrowserWindowInterface* bwi,
                        bool prevent_close,
                        mojom::InvocationSource source);

  // Invokes Glic with the given options and automatically submits the prompt.
  // Access is restricted to authorized callers via InvokeWithAutoSubmitPasskey.
  // Virtual for testing.
  virtual base::WeakPtr<GlicInstance> InvokeWithAutoSubmit(
      InvokeWithAutoSubmitPasskey auto_submit_passkey,
      GlicInvokeOptions options);

  virtual base::WeakPtr<GlicInstance> InvokeWithAutoSubmit(
      InvokeWithAutoSubmitPasskey auto_submit_passkey,
      GlicInvokeOptions options,
      GlicInvokeWithAutoSubmitOptions auto_submit_options);

  virtual base::WeakPtr<GlicInstance> Invoke(GlicInvokeOptions options);

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
  virtual GlicFreController& fre_controller();
#if !BUILDFLAG(IS_ANDROID)
  virtual GlicExperimentalOptInController& opt_in_controller();
#endif
  virtual GlicInstanceCoordinator& instance_coordinator() const;

  // Return a `GlicActiveInstanceSharingManager` which tracks the sharing state
  // for whichever instance is active. Please prefer to use the sharing manager
  // on the `GlicInstance` if you don't need one that automatically tracks the
  // active instance.
  GlicSharingManager& active_instance_sharing_manager();

  // Returns true if `bwi` has a glic panel showing for its active tab. Virtual
  // for testing.
  virtual bool IsPanelShowingForBrowser(
      const BrowserWindowInterface& bwi) const;

  // Virtual for testing.
  virtual bool IsWindowDetached() const;

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

  // CreateTab is used by both the FRE page and the glic web client to open a
  // URL in a new tab. The source is the RenderFrameHost of the Glic
  // instance that is requesting the navigation - this gets set as the
  // navigation handle's opener param.
  tabs::TabInterface* CreateTab(
      const ::GURL& url,
      bool open_in_background,
      const std::optional<int32_t>& window_id,
      glic::mojom::WebClientHandler::CreateTabCallback callback);

  void OnUserInputSubmitted(glic::mojom::WebClientMode mode);

  // Registers a callback to be called any time user input is submitted in the
  // client. This is used to update UI effects on tabs that are being shared
  // with glic.
  base::CallbackListSubscription AddUserInputSubmittedCallback(
      base::RepeatingClosure callback);

  // Fetches the image for the context menu item (if possible, and potentially
  // scaling and reencoding) and sends the result to the web client as
  // additional data.
  void ShareContextImage(tabs::TabInterface* tab,
                         content::RenderFrameHost* frame,
                         const ::GURL& src_url);

  AuthController& GetAuthController() { return *auth_controller_; }

  void AddPreloadCallback(base::OnceCallback<void()> callback);

  virtual void TryPreload();
  void TryPreloadAfterDelay();
  void Reload(content::RenderFrameHost* render_frame_host);
  // Close the active embedder for an instance associated with this render frame
  // host.
  void Close(content::RenderFrameHost* outermost_render_frame_host);
  // Archive the active embedder for an instance associated with this render
  // frame host.
  void Archive(content::RenderFrameHost* outermost_render_frame_host);
  Profile* profile() const { return profile_; }

  // Used only for testing purposes.
  void reset_profile_for_test() { profile_ = nullptr; }

  base::WeakPtr<GlicKeyedService> GetWeakPtr();

  // Get the GlicInstance associated with the given browser's active tab, or
  // null if there is none. `bwi` can be null if preloaded with no browser open.
  GlicInstance* GetInstanceForActiveTab(BrowserWindowInterface* bwi);

  // Get the GlicInstance for a provided tab, or null if there is none.
  virtual GlicInstance* GetInstanceForTab(tabs::TabInterface* tab);

  // Sends additional context to the web client associated with the given tab.
  // If no web client exists for the tab, then this method does nothing. It is
  // the responsibility of the caller to ensure that a host exists before
  // calling this method.
  virtual void SendAdditionalContext(tabs::TabHandle tab_handle,
                                     mojom::AdditionalContextPtr context);

  GlicTabDataObserver& tab_data_observer() { return *tab_data_observer_; }
  GlicTabFaviconObserver& tab_favicon_observer() {
    return *tab_favicon_observer_;
  }

  using ActOnWebCapabilityChangedCallback = base::RepeatingCallback<void(bool)>;
  base::CallbackListSubscription AddActOnWebCapabilityChangedCallback(
      ActOnWebCapabilityChangedCallback callback);

  // Virtual for testing.
  virtual GlicActorPolicyChecker& actor_policy_checker();

 private:
  // A helper function to route GetZeroStateSuggestionsForFocusedTabCallback
  // callbacks.
  void OnZeroStateSuggestionsFetched(
      glic::mojom::ZeroStateSuggestionsPtr suggestions,
      glic::mojom::WebClientHandler::
          GetZeroStateSuggestionsForFocusedTabCallback callback,
      std::vector<std::string> returned_suggestions);

  // Shared implementation for ToggleUI.
  void ToggleUIInternal(BrowserWindowInterface* bwi,
                        bool prevent_close,
                        mojom::InvocationSource source,
                        std::optional<std::string> prompt_suggestion);

  bool MaybeInvoke(BrowserWindowInterface* bwi,
                   mojom::InvocationSource source,
                   const std::optional<std::string>& prompt_suggestion);

  void InitializeAfterConstruction();

  void FinishPreload(GlicPrewarmingChecksResult reason);

  // List of callbacks to be notified when the client requests a change to the
  // context access indicator status.
  base::RepeatingCallbackList<void(bool)>
      context_access_indicator_callback_list_;
  // The state of the context access indicator as set by the client.
  bool is_context_access_indicator_enabled_ = false;

  // List of callbacks to be notified when user input has been submitted.
  base::RepeatingClosureList user_input_submitted_callback_list_;

  raw_ptr<Profile> profile_;

  // Never null - GlicActorTaskManager and GlicInstanceCoordinatorImpl hold a
  // reference to this so it must be destroyed after them.
  std::unique_ptr<GlicActorPolicyChecker> actor_policy_checker_;

  std::unique_ptr<GlicEnabling> enabling_;
  std::unique_ptr<GlicMetrics> metrics_;
  std::unique_ptr<GlicFreController> fre_controller_;
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<GlicExperimentalOptInController> opt_in_controller_;
#endif
  // Is a GlicInstanceCoordinatorImpl.
  std::unique_ptr<GlicInstanceCoordinator> instance_coordinator_;
  std::unique_ptr<GlicSharingManager> sharing_manager_;
  std::unique_ptr<GlicShareImageHandler> share_image_handler_;

  std::unique_ptr<AuthController> auth_controller_;

  base::OnceCallback<void()> preload_callback_;

  std::unique_ptr<GlicTabDataObserver> tab_data_observer_;
  std::unique_ptr<GlicTabFaviconObserver> tab_favicon_observer_;

  base::WeakPtrFactory<GlicKeyedService> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_GLIC_KEYED_SERVICE_H_
