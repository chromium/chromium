// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_IMPL_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_IMPL_H_

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/glic/actor/glic_actor_task_manager.h"
#include "chrome/browser/glic/host/context/glic_delegating_sharing_manager.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_provider.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/glic/service/glic_ui_types.h"
#include "chrome/browser/glic/service/metrics/glic_instance_metrics.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/autofill/core/browser/integrators/glic/actor_form_filling_types.h"
#include "components/tabs/public/tab_interface.h"

class Profile;

namespace tabs {
class TabInterface;
}
namespace contextual_cueing {
class ContextualCueingService;
}

namespace glic {

class GlicUiEmbedder;
class EmptyEmbedderDelegate;
class GlicTabContentsObserver;
class GlicZeroStateSuggestionsManager;

// A GlicInstance owns a single host keeping any state that must exist for the
// lifetime of the host. When a host is showing, the GlicInstance creates a
// GlicUiEmbedder to display the webcontents in. An instance (and host) exist
// even if it has no GlicUiEmbedder showing the UI. A host could have many
// different GlicUiEmbedders during its lifetime.
class GlicInstanceImpl : public GlicInstance,
                         public BrowserListObserver,
                         public Host::InstanceDelegate,
                         public Host::Observer,
                         public GlicSharingManagerProvider,
                         public GlicUiEmbedder::Delegate,
                         public actor::ActorTaskDelegate {
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
    GetRecentlyActiveConversations() = 0;

    // Called when the context access indicator changes on the instance.
    virtual void ContextAccessIndicatorChanged(
        GlicInstanceImpl& source_instance,
        bool enabled) = 0;
  };

  GlicInstanceImpl(
      Profile* profile,
      InstanceId instance_id,
      base::WeakPtr<InstanceCoordinatorDelegate> coordinator_delegate,
      GlicMetrics* metrics,
      contextual_cueing::ContextualCueingService* contextual_cueing_service);
  ~GlicInstanceImpl() override;

  GlicInstanceImpl(const GlicInstanceImpl&) = delete;
  GlicInstanceImpl& operator=(const GlicInstanceImpl&) = delete;

  Profile* profile() { return profile_; }

  // Returns whether host's webcontents are focused.
  bool HasFocus();

  // GlicSharingManagerProvider implementation.
  GlicSharingManager& sharing_manager() override;

  void NotifyInstanceActivationChanged(bool is_active);

  base::TimeTicks GetLastActiveTime() const override;

  bool IsHibernated() const;

  void Hibernate();

  void CloseInstanceAndShutdown();

  // GlicInstance implementation.
  bool IsShowing() const override;
  bool IsAttached() override;
  gfx::Size GetPanelSize() override;
  bool IsActive() override;

  bool IsDetached();
  bool IsActuating() const;
  bool IsLiveMode();

  glic::mojom::ConversationInfoPtr GetConversationInfo() const;

  // These methods should only be called by the GlicInstanceCoordinator.
  // This method will either show an embedder or create an inactive embedder and
  // bind a tab to conversation.
  void Show(const ShowOptions& options) override;
  void Close(EmbedderKey key);
  // Returns true when toggle shows the instance and false when it is closed.
  bool Toggle(ShowOptions&& options,
              bool prevent_close,
              glic::mojom::InvocationSource source);

  void UnbindEmbedder(EmbedderKey key);
  GlicUiEmbedder* GetEmbedderForTab(tabs::TabInterface* tab);
  bool ContextAccessIndicatorEnabled();
  void CloseAllEmbedders();

  // GlicInstance:
  Host& host() override;
  const InstanceId& id() const override;
  std::optional<std::string> conversation_id() const override;
  base::CallbackListSubscription RegisterStateChange(
      StateChangeCallback callback) override;

  // Host::InstanceDelegate:
  // TODO: Currently, both GlicInstanceImpl and GlicKeyedService implement
  // Host::InstanceDelegate. The CreateTab function here should only return the
  // tab for GlicKeyedService, but not GlicInstanceImpl. We should figure out a
  // way to decouple this.
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
  void OnInteractionModeChange(mojom::WebClientMode new_mode) override;
  glic::GlicInstanceMetrics* instance_metrics() override;

  // GlicUiEmbedder::Delegate:
  void OnEmbedderWindowActivationChanged(bool has_focus) override;
  void SwitchConversation(
      const ShowOptions& options,
      glic::mojom::ConversationInfoPtr info,
      mojom::WebClientHandler::SwitchConversationCallback callback) override;
  void WillCloseFor(EmbedderKey key) override;
  void NotifyPanelStateChanged() override;
  // Opens the floating UI for this instance
  void Detach(tabs::TabInterface& tab) override;
  void Attach(tabs::TabInterface& tab) override;

  // Host::InstanceInterface:
  mojom::PanelState GetPanelState() override;
  void AddStateObserver(PanelStateObserver* observer) override;
  void RemoveStateObserver(PanelStateObserver* observer) override;

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;

  // Host::Observer
  void ClientReadyToShow(const mojom::OpenPanelInfo& open_info) override;
  void WebUiStateChanged(mojom::WebUiState state) override;
  void ContextAccessIndicatorChanged(bool enabled) override;

  glic::GlicInstanceMetrics* metrics() { return &instance_metrics_; }

  // Test support.
  void CloseAllEmbeddersForTesting();
  views::View* GetActiveEmbedderGlicViewForTesting();
  std::string DescribeForTesting();

  // ActorTaskDelegate:
  void OnTabAddedToTask(actor::TaskId task_id,
                        const tabs::TabInterface::Handle& tab_handle) override;
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
  // We use a delegating constructor pattern so we can hand off ownership of the
  // focused browser manager as well as provide a reference to it to another
  // object.
  GlicInstanceImpl(
      Profile* profile,
      InstanceId instance_id,
      base::WeakPtr<InstanceCoordinatorDelegate> coordinator_delegate,
      GlicMetrics* metrics,
      contextual_cueing::ContextualCueingService* contextual_cueing_service,
      GlicFocusedBrowserManager* detached_mode_focused_browser_manager,
      GlicFocusedBrowserManager* live_mode_focused_browser_manager);

  struct EmbedderEntry {
    EmbedderEntry();
    ~EmbedderEntry();
    EmbedderEntry(EmbedderEntry&&);
    EmbedderEntry& operator=(EmbedderEntry&&);

    std::unique_ptr<GlicUiEmbedder> embedder;
    base::CallbackListSubscription destruction_subscription;
    base::CallbackListSubscription tab_activation_subscription;
    std::unique_ptr<GlicTabContentsObserver> tab_web_contents_observer;
  };

  struct ConversationInfo {
    std::string conversation_id;
    std::string conversation_title;
  };

  void NotifyStateChange();

  GlicUiEmbedder* GetActiveEmbedder();
  GlicUiEmbedder* GetEmbedderForKey(EmbedderKey key);
  void DeactivateCurrentEmbedder();
  void OnAllEmbeddersInactive();
  GlicUiEmbedder* CreateActiveEmbedder(const ShowOptions& options);
  GlicUiEmbedder* CreateActiveEmbedderForSidePanel(
      const SidePanelShowOptions& options);
  GlicUiEmbedder* CreateActiveEmbedderForFloaty(
      const gfx::Rect& initial_bounds,
      tabs::TabInterface::Handle source_tab);
  void ShowInactiveSidePanelEmbedderFor(const SidePanelShowOptions& options);
  void SetActiveEmbedderAndNotifyStateChange(
      std::optional<EmbedderKey> new_key);
  void ClearActiveEmbedderAndNotifyStateChange();
  void MaybeShowHostUi(GlicUiEmbedder* embedder);
  void OnBoundTabDestroyed(tabs::TabInterface* tab,
                           const InstanceId& instance_id);
  void OnBoundTabActivated(tabs::TabInterface* tab);
  bool ShouldDoAutomaticActivation() const;
  void OnZeroStateSuggestionsFetched(
      mojom::ZeroStateSuggestionsPtr suggestions,
      mojom::WebClientHandler::GetZeroStateSuggestionsForFocusedTabCallback
          callback,
      std::vector<std::string> returned_suggestions);
  void MaybeDeactivateEmbedder(EmbedderKey key);

  bool ShouldPinOnBind() const;

  void MaybeActivateForegroundEmbedder();
  void MaybeRemoveBlankInstanceOnClose();
  EmbedderEntry& BindTab(tabs::TabInterface* tab, GlicPinTrigger pin_trigger);
  // For any pinned tab not already bound to a conversation bind it to this one.
  void OnTabPinningStatusChanged(tabs::TabInterface* tab, bool pinned);
  void NotifyPanelWillOpen(mojom::InvocationSource invocation_source);

  void UpdateSharingManagerDelegate();

  using StateChangeCallbackList =
      base::RepeatingCallbackList<void(bool, mojom::CurrentView view)>;
  StateChangeCallbackList state_change_callback_list_;

  base::ObserverList<PanelStateObserver> state_observers_;

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
  std::optional<ConversationInfo> conversation_info_;

  // The pinned tab manager for the instance.
  // TODO (crbug.com/452150693): move ownership of this instance into the
  // GlicStablePinningDelegatingSharingManager.
  GlicPinnedTabManager pinned_tab_manager_;

  // The sharing manager used internally for detached mode.
  GlicSharingManagerImpl detached_mode_sharing_manager_;

  // The sharing manager used internally for live mode.
  GlicSharingManagerImpl live_mode_sharing_manager_;

  // The sharing manager used internally for attached mode.
  GlicSharingManagerImpl attached_mode_sharing_manager_;

  // The source of truth sharing manager for the instance.
  GlicStablePinningDelegatingSharingManager sharing_manager_;

  // GlicInstanceMetrics ctor requires the sharing_manager_ above, so it must be
  // declared after it to prevent memory errors.
  GlicInstanceMetrics instance_metrics_;

  // Tracks the last non-hidden panel state kind for the instance. This is
  // useful for responding to changes in attached/detached state.
  mojom::PanelStateKind last_non_hidden_panel_state_kind_ =
      mojom::PanelStateKind::kAttached;
  mojom::WebClientMode interaction_mode_ = mojom::WebClientMode::kText;

  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};
  base::ScopedObservation<Host, Host::Observer> host_observation_{this};

  std::unique_ptr<GlicZeroStateSuggestionsManager>
      zero_state_suggestions_manager_;
  std::unique_ptr<GlicActorTaskManager> actor_task_manager_;
  base::CallbackListSubscription pinned_tabs_change_subscription_;

  base::OneShotTimer inactivity_timer_;
  base::TimeTicks last_active_time_;

  base::OneShotTimer remove_blank_instance_timer_;

  base::WeakPtrFactory<GlicInstanceImpl> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_IMPL_H_
