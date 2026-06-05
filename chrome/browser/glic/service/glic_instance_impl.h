// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_IMPL_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_IMPL_H_

#include <memory>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/glic/actor/glic_actor_task_manager.h"
#include "chrome/browser/glic/host/context/glic_delegating_sharing_manager.h"
#include "chrome/browser/glic/host/context/glic_focused_browser_manager.h"
#include "chrome/browser/glic/host/context/glic_pinned_tab_manager.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_coordinator.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_provider.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/glic/service/glic_ui_types.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#endif

class GlobalBrowserCollection;
class Profile;

namespace tabs {
class TabInterface;
}

namespace glic {
class ContextualCueingService;
class GlicMetrics;
class GlicUiEmbedder;
class EmptyEmbedderDelegate;
class GlicSkillsManagerImpl;
class GlicZeroStateSuggestionsManager;

BASE_DECLARE_FEATURE(kGlicRemoveDaisyChainingWhenFreShowing);
BASE_DECLARE_FEATURE(kGlicUnbindOnClose);

// A GlicInstance owns a single host keeping any state that must exist for the
// lifetime of the host. When a host is showing, the GlicInstance creates a
// GlicUiEmbedder to display the webcontents in. An instance (and host) exist
// even if it has no GlicUiEmbedder showing the UI. A host could have many
// different GlicUiEmbedders during its lifetime.
class GlicInstanceImpl : public GlicInstance,
                         public GlicInstanceHelper::Instance,
                         public BrowserCollectionObserver,
                         public Host::InstanceDelegate,
                         public Host::Observer,
                         public GlicSharingManagerProvider,
                         public GlicUiEmbedder::Delegate,
                         public GlicActorTaskManager::Delegate {
 public:
  class InstanceCoordinatorDelegate {
   public:
    virtual ~InstanceCoordinatorDelegate() = default;
    virtual void RemoveInstance(GlicInstanceImpl* instance) = 0;
    // Called by an instance when its visibility state changes.
    virtual void OnInstanceVisibilityChanged(GlicInstanceImpl* instance,
                                             bool is_showing) = 0;
    virtual void OnInstanceActivationChanged(GlicInstanceImpl* instance,
                                             bool is_active) = 0;
    virtual void SwitchConversation(
        GlicInstanceImpl& source_instance,
        const ShowOptions& options,
        glic::mojom::ConversationInfoPtr info,
        mojom::WebClientHandler::SwitchConversationCallback callback) = 0;

    virtual void UnbindTabFromAnyInstance(tabs::TabInterface* tab) = 0;

    // Called by an instance when user requests to undock to Floaty.
    virtual void OnWillCreateFloaty() = 0;

    virtual std::vector<glic::mojom::ConversationInfoPtr>
    GetRecentlyActiveConversations(size_t limit) = 0;

    // Called when the context access indicator changes on the instance.
    virtual void ContextAccessIndicatorChanged(
        GlicInstanceImpl& source_instance,
        bool enabled) = 0;

    // Called to create a new web contents for the glic instance.
    virtual std::unique_ptr<WebUIContentsContainer>
    CreateWebUIContentsContainer() = 0;
  };

  GlicInstanceImpl(
      Profile* profile,
      InstanceId instance_id,
      base::WeakPtr<InstanceCoordinatorDelegate> coordinator_delegate,
      GlicMetrics* metrics,
      ContextualCueingService* contextual_cueing_service);
  ~GlicInstanceImpl() override;

  GlicInstanceImpl(const GlicInstanceImpl&) = delete;
  GlicInstanceImpl& operator=(const GlicInstanceImpl&) = delete;

  Profile* profile() { return profile_; }
  GlicKeyedService* service() { return service_; }

  // Returns whether host's webcontents are focused.
  bool HasFocus();
  GlicUiEmbedder* GetActiveEmbedder();

  // GlicSharingManagerProvider implementation.
  GlicSharingManager& sharing_manager() override;
  GlicPinCandidateProvider& pin_candidate_provider() override;

  void NotifyInstanceActivationChanged(bool is_active);
  base::Time GetLastActivationTimestamp() const override;
  base::TimeDelta GetTimeSinceLastActive() const override;
  base::TimeDelta GetTimeSinceLastPromptSubmission() const override;
  bool IsHibernated() const;
  void Hibernate();
  void Shutdown();
  void CloseInstanceAndShutdown();
  void BindTabWithoutShowing(tabs::TabInterface* tab,
                             GlicPinTrigger pin_trigger,
                             bool pin_on_bind);
  void SuppressShowOnNextTabAddedToTask(bool suppress);
  // Initializes the instance for a hidden client. No-op if the instance already
  // has webui contents.
  void MaybeInitializeHiddenClient(mojom::InvocationSource invocation_source,
                                   mojom::FreOverride fre_override);

  // GlicInstance implementation.
  bool IsShowing() const override;
  gfx::Size GetPanelSize() override;
  std::optional<Target> GetInvokeTarget() override;
  bool IsActive() override;

  bool HasActiveEmbedder() const;
  bool IsDetached();
  bool IsActuating() const override;
  bool IsLiveMode();

  glic::mojom::ConversationInfoPtr GetConversationInfo() const;

  // These methods should only be called by the GlicInstanceCoordinator.
  // This method will either show an embedder or create an inactive embedder and
  // bind a tab to conversation.
  void Show(const ShowOptions& options) override;

  // Called when a new tab is created from a source tab that is bound to this
  // instance. This attempts to daisy chain the new tab to the same instance.
  void MaybeDaisyChainToTab(tabs::TabInterface* source_tab,
                            tabs::TabInterface* target_tab,
                            DaisyChainSource source);

  // Closes the embedder identified by `key`.
  // NOTE: This method may result in the deletion of `this`.
  void Close(EmbedderKey key, const CloseOptions& options = {});
  // Returns true when toggle shows the instance and false when it is closed.
  bool Toggle(ShowOptions&& options,
              bool prevent_close,
              glic::mojom::InvocationSource source);

  // NOTE: This method may result in the deletion of `this`.
  void UnbindEmbedder(EmbedderKey key);
  GlicUiEmbedder* GetEmbedderForTab(tabs::TabInterface* tab);
  bool ContextAccessIndicatorEnabled();
  void CloseAllEmbedders();
  Host& host() override;

  // GlicInstance:
  void SendAdditionalContext(mojom::AdditionalContextPtr context) override;
  void FocusIfActive() override;
  void NotifyActorTaskListRowClicked(int32_t task_id) override;
  void GetExperimentalTriggeringUpdates(
      mojo::PendingRemote<mojom::ExperimentalTriggeringUpdatesHandler> handler,
      base::OnceCallback<void(bool)> success_status_callback) override;
  const InstanceId& id() const override;
  void SetIdForRestoration(InstanceId id);
  std::optional<std::string> conversation_id() const override;
  std::string conversation_title() const override;
  std::vector<tabs::TabInterface*> GetBoundTabs() const;
  base::CallbackListSubscription AddConversationInfoChangedCallback(
      base::RepeatingCallback<void(const mojom::ConversationInfo&)> callback);
  void CancelTask() override;
  GlicActorTaskManager* GetActorTaskManager() override;

  // Called exactly once, right before the instance is destroyed.
  using DestructionCallback = base::OnceCallback<void(GlicInstance*)>;
  base::CallbackListSubscription RegisterWillBeDestroyed(
      DestructionCallback callback) override;

  // Host::InstanceDelegate:
  void CreateTab(
      const ::GURL& url,
      bool open_in_background,
      const std::optional<int32_t>& window_id,
      glic::mojom::WebClientHandler::CreateTabCallback callback) override;
  void FetchZeroStateSuggestions(
      bool is_first_run,
      std::optional<std::vector<std::string>> supported_tools,
      glic::mojom::WebClientHandler::
          GetZeroStateSuggestionsForFocusedTabCallback callback) override;
  void RegisterConversation(
      glic::mojom::ConversationInfoPtr info,
      mojom::WebClientHandler::RegisterConversationCallback callback) override;
  void GetZeroStateSuggestionsAndSubscribe(
      bool has_active_subscription,
      const mojom::ZeroStateSuggestionsOptions& options,
      mojom::WebClientHandler::GetZeroStateSuggestionsAndSubscribeCallback
          callback) override;
  void OnWebClientCleared() override;
  void PrepareForOpen() override;
  void OnUserInputSubmitted(mojom::WebClientMode mode) override;
  void OnInteractionModeChange(mojom::WebClientMode new_mode) override;
  glic::GlicInstanceMetrics& instance_metrics() override;
  glic::GlicInstanceMetricsBackwardsCompatibility&
  instance_metrics_backwards_compatibility() override;
  GlicSkillsManager& skills_manager() override;

  std::unique_ptr<WebUIContentsContainer> CreateWebUIContentsContainer()
      override;
  void CreateActorHandler(
      mojo::PendingReceiver<mojom::ActorHandler> receiver,
      mojo::PendingRemote<mojom::ActorClient> client) override;

  // GlicUiEmbedder::Delegate:
  void OnEmbedderWindowActivationChanged(bool has_focus) override;
  void SwitchConversation(
      const ShowOptions& options,
      glic::mojom::ConversationInfoPtr info,
      mojom::WebClientHandler::SwitchConversationCallback callback) override;
  // Notifies that the embedder has closed. Guaranteed to be called for all
  // close sources.
  void DidCloseFor(EmbedderKey key, EmbedderCloseReason reason) override;
  void NotifyPanelStateChanged() override;
  // Opens the floating UI for this instance
  void Detach(tabs::TabInterface& tab) override;
  void Attach(tabs::TabHandle tab) override;

  // Host::InstanceInterface:
  mojom::PanelState GetPanelState() override;
  void AddStateObserver(PanelStateObserver* observer) override;
  void RemoveStateObserver(PanelStateObserver* observer) override;

  // BrowserCollectionObserver:
  void OnBrowserActivated(BrowserWindowInterface* browser) override;

  // Host::Observer
  void ClientReadyToShow(const mojom::OpenPanelInfo& open_info) override;
  void WebUiStateChanged(mojom::WebUiState state) override;
  void ContextAccessIndicatorChanged(bool enabled) override;

  // Test support.
#if !BUILDFLAG(IS_ANDROID)
  views::View* GetActiveEmbedderGlicViewForTesting();
  class GlicFloatingUi* GetFloatingUiForTesting();
#endif
  tabs::TabInterface* GetActiveEmbedderTabForTesting();
  std::string DescribeForTesting();

  base::WeakPtr<GlicInstanceImpl> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // GlicActorTaskManager::Delegate:
  void OnTabAddedToTask(actor::TaskId task_id,
                        const tabs::TabInterface::Handle& tab_handle) override;

 private:
  struct EmbedderEntry {
    EmbedderEntry();
    ~EmbedderEntry();
    EmbedderEntry(EmbedderEntry&&);
    EmbedderEntry& operator=(EmbedderEntry&&);

    std::unique_ptr<GlicUiEmbedder> embedder;
    base::CallbackListSubscription destruction_subscription;
    base::CallbackListSubscription tab_activation_subscription;
    bool user_input_submitted_while_bound = false;
  };

  void NotifyVisibilityChange();
  void NotifyConversationTitleChanged();

  GlicUiEmbedder* GetEmbedderForKey(EmbedderKey key);
  EmbedderEntry* GetEmbedderEntry(EmbedderKey key);
  void DeactivateCurrentEmbedder();
  void OnAllEmbeddersInactive();
  GlicUiEmbedder* CreateActiveEmbedder(const ShowOptions& options);
  GlicUiEmbedder* CreateActiveEmbedderForSidePanel(
      const SidePanelShowOptions& options);
  GlicUiEmbedder* CreateActiveEmbedderForFloaty(
      const gfx::Rect& initial_bounds,
      tabs::TabInterface::Handle source_tab);
  void ShowInactiveSidePanelEmbedderFor(const SidePanelShowOptions& options);
  void SetActiveEmbedderAndNotifyVisibilityChange(
      std::optional<EmbedderKey> new_key);
  void ClearActiveEmbedderAndNotifyVisibilityChange();
  void CloseInternal(EmbedderKey key,
                     EmbedderEntry& entry,
                     const CloseOptions& options = {});
  bool ShouldUnbindOnClose(EmbedderKey key, const EmbedderEntry& entry);
  void MaybeShowHostUi(
      GlicUiEmbedder* embedder,
      mojom::InvocationSource source,
      std::optional<std::string> prompt_suggestion,
      mojom::FreOverride fre_override = mojom::FreOverride::kUnspecified);
  void OnBoundTabDestroyed(tabs::TabInterface* tab);
  void OnBoundTabActivated(tabs::TabInterface* tab);
  bool ShouldDoAutomaticActivation() const;
  void OnZeroStateSuggestionsFetched(
      mojom::ZeroStateSuggestionsPtr suggestions,
      mojom::WebClientHandler::GetZeroStateSuggestionsForFocusedTabCallback
          callback,
      std::vector<std::string> returned_suggestions);
  void MaybeDeactivateEmbedder(EmbedderKey key);
  void MaybeWarmZeroStateSuggestions();

  bool IsActiveEmbedder(EmbedderKey key) const;
  bool ShouldShowInactiveSidePanel(const SidePanelShowOptions& options) const;

  bool ShouldPinOnBind() const;

  void MaybeActivateForegroundEmbedder();
  void MaybeRemoveBlankInstanceOnClose();
  EmbedderEntry& BindTab(tabs::TabInterface* tab,
                         GlicPinTrigger pin_trigger,
                         bool pin_on_bind);
  // For any pinned tab not already bound to a conversation bind it to this one.
  void OnTabPinningStatusEvent(tabs::TabInterface* tab,
                               GlicPinningStatusEvent event);
  void NotifyPanelWillOpen(
      mojom::InvocationSource invocation_source,
      std::optional<std::string> prompt_suggestion,
      mojom::FreOverride fre_override = mojom::FreOverride::kUnspecified);

  void MaybeShowShortcutSnoozePromo();

  // Updates the floating panel can attach state.
  void UpdateFloatingPanelCanAttach();

  using ConversationInfoChangedCallbackList =
      base::RepeatingCallbackList<void(const mojom::ConversationInfo&)>;
  ConversationInfoChangedCallbackList conversation_info_changed_callback_list_;

  base::ObserverList<PanelStateObserver> state_observers_;

  base::OnceCallbackList<void(GlicInstance*)> will_be_destroyed_callbacks_;

  raw_ptr<Profile> profile_;
  raw_ptr<GlicKeyedService> service_;

  base::WeakPtr<InstanceCoordinatorDelegate> coordinator_delegate_;
  InstanceId id_;

  // The single source of truth for all embedders.
  // A tabs::TabInterface* key is a tab-bound side panel.
  // A FloatingEmbedderKey key is the instance-bound floating panel.
  base::flat_map<EmbedderKey, EmbedderEntry> embedders_;

  // The single, unambiguous source of truth for the active UI.
  std::optional<EmbedderKey> active_embedder_key_;

  // The empty embedder delegate is owned by this instance and its lifetime is
  // guaranteed to be longer than `host_` because it is declared before `host_`.
  glic::EmptyEmbedderDelegate empty_embedder_delegate_;
  // `IsActive` can be called by `host_`, so the member needs to outlive it.
  bool is_active_ = false;
  Host host_;
  // Never null
  mojom::ConversationInfoPtr conversation_info_ =
      mojom::ConversationInfo::New();

  GlicSharingManagerCoordinator sharing_manager_coordinator_;

  // GlicInstanceMetrics ctor requires the sharing_manager_ above, so it must be
  // declared after it to prevent memory errors.
  GlicInstanceMetrics instance_metrics_;

  mojom::WebClientMode interaction_mode_ = mojom::WebClientMode::kText;

  base::ScopedObservation<GlobalBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
  base::ScopedObservation<Host, Host::Observer> host_observation_{this};

  std::unique_ptr<GlicZeroStateSuggestionsManager>
      zero_state_suggestions_manager_;
  std::unique_ptr<GlicSkillsManagerImpl> skills_manager_;
  std::unique_ptr<GlicActorTaskManager> actor_task_manager_;
  base::CallbackListSubscription pinned_tabs_change_subscription_;

  base::OneShotTimer inactivity_timer_;
  base::Time last_activation_timestamp_;
  base::TimeTicks last_deactivation_timestamp_;
  base::TimeTicks last_prompt_submission_time_;

  base::OneShotTimer remove_blank_instance_timer_;
  base::OneShotTimer maybe_activate_foreground_embedder_timer_;

  // True during the synchronous execution of `CreateTab()`, which is called
  // when the user clicks a link inside the Glic panel.
  bool is_creating_tab_from_glic_panel_link_click_ = false;

  // True if we should suppress showing the panel when a tab is added to a task.
  bool suppress_show_on_tab_added_to_task_ = false;

  base::WeakPtrFactory<GlicInstanceImpl> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_IMPL_H_
