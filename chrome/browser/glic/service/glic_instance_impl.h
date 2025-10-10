// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_IMPL_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_IMPL_H_

#include <variant>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_provider.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/service/glic_instance_helper.h"
#include "chrome/browser/glic/service/glic_ui_embedder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "components/tabs/public/tab_interface.h"

class Profile;

namespace tabs {
class TabInterface;
}

namespace glic {

class GlicUiEmbedder;
class EmptyEmbedderDelegate;
class GlicTabContentsObserver;

// A GlicInstance owns a single host keeping any state that must exist for the
// lifetime of the host. When a host is showing, the GlicInstance creates a
// GlicUiEmbedder to display the webcontents in. An instance (and host) exist
// even if it has no GlicUiEmbedder showing the UI. A host could have many
// different GlicUiEmbedders during its lifetime.
class GlicInstanceImpl : public GlicInstance,
                         public BrowserListObserver,
                         public Host::InstanceDelegate,
                         public Host::Observer,
                         public Host::InstanceInterfaceForMigration,
                         public GlicSharingManagerProvider,
                         public GlicUiEmbedder::Delegate {
 public:
  enum class EmbedderType {
    kSidePanel,
    kFloating,
  };

  class InstanceCoordinatorDelegate {
   public:
    virtual ~InstanceCoordinatorDelegate() = default;
    virtual void OnInstanceOrphaned(GlicInstance* instance) = 0;
    // Called by an instance when its visibility state changes.
    virtual void OnInstanceVisibilityChanged(GlicInstance* instance,
                                             bool is_showing) = 0;
    virtual void SwitchConversation(
        tabs::TabInterface* tab,
        glic::mojom::ConversationInfoPtr info,
        mojom::WebClientHandler::SwitchConversationCallback callback) = 0;
  };

  GlicInstanceImpl(
      Profile* profile,
      InstanceId instance_id,
      base::WeakPtr<InstanceCoordinatorDelegate> coordinator_delegate,
      GlicMetrics* metrics);
  ~GlicInstanceImpl() override;

  GlicInstanceImpl(const GlicInstanceImpl&) = delete;
  GlicInstanceImpl& operator=(const GlicInstanceImpl&) = delete;

  Profile* profile() { return profile_; }

  // GlicSharingManagerProvider implementation.
  GlicSharingManager& sharing_manager() override;

  void CloseInstanceAndShutdown();

  // GlicInstance implementation.
  bool IsShowing() const override;
  bool IsAttached() override;
  gfx::Size GetPanelSize() override;

  // These methods should only be called by the GlicInstanceCoordinator.
  void Show(EmbedderType type, tabs::TabInterface* tab);
  void Close(EmbedderType type, tabs::TabInterface* tab);
  void Toggle(EmbedderType type, tabs::TabInterface* tab, bool prevent_close);

  void UnbindTab(tabs::TabInterface* tab);
  bool IsOrphaned() const;
  GlicUiEmbedder* GetEmbedderForTab(tabs::TabInterface* tab);

  // GlicInstance:
  Host& host() override;
  const InstanceId& id() const override;
  std::optional<std::string> conversation_id() const;
  base::CallbackListSubscription RegisterStateChange(
      StateChangeCallback callback) override;

  // Host::InstanceDelegate:
  void CreateTab(
      content::RenderFrameHost* source,
      const ::GURL& url,
      bool open_in_background,
      const std::optional<int32_t>& window_id,
      glic::mojom::WebClientHandler::CreateTabCallback callback) override;
  void CreateTask(
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
  void PrepareForOpen() override;

  // GlicUiEmbedder::Delegate:
  void SwitchConversation(
      tabs::TabInterface* tab,
      glic::mojom::ConversationInfoPtr info,
      mojom::WebClientHandler::SwitchConversationCallback callback) override;
  void WillCloseFor(tabs::TabInterface* tab) override;
  void Attach(tabs::TabInterface* tab) override;
  void NotifyPanelStateChanged() override;
  // Opens the floating UI for this instance
  void Detach(tabs::TabInterface* tab) override;

  // Host::InstanceInterface:
  mojom::PanelState GetPanelState() override;
  void AddStateObserver(PanelStateObserver* observer) override;
  void RemoveStateObserver(PanelStateObserver* observer) override;

  // BrowserListObserver:
  void OnBrowserSetLastActive(Browser* browser) override;

  // Host::Observer
  void WebUiStateChanged(mojom::WebUiState state) override;

 private:
  // A tag type to represent the floating embedder key.
  struct FloatingEmbedderKey {
    auto operator<=>(const FloatingEmbedderKey&) const = default;
  };

  using EmbedderKey = std::variant<FloatingEmbedderKey, tabs::TabInterface*>;

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

  EmbedderKey GetEmbedderKey(EmbedderType type, tabs::TabInterface* tab);
  GlicUiEmbedder* GetActiveEmbedder();
  GlicUiEmbedder* GetEmbedderForKey(EmbedderKey key);
  void DeactivateCurrentEmbedder();
  GlicUiEmbedder* CreateActiveEmbedderForSidePanel(tabs::TabInterface* tab);
  GlicUiEmbedder* CreateActiveEmbedderForFloaty(
      BrowserWindowInterface* browser);
  void ShowInactiveSidePanelEmbedderFor(tabs::TabInterface* tab);
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
  void MaybeDeactivateEmbedderAndCloseHostUi(EmbedderKey key);

  void MaybeActivateForegroundEmbedder();
  EmbedderEntry& BindTab(tabs::TabInterface* tab);

  using StateChangeCallbackList =
      base::RepeatingCallbackList<void(bool, mojom::CurrentView view)>;
  StateChangeCallbackList state_change_callback_list_;

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
  Host host_;
  std::optional<ConversationInfo> conversation_info_;
  GlicSharingManagerImpl sharing_manager_;

  base::ObserverList<PanelStateObserver> state_observers_;

  base::ScopedObservation<BrowserList, BrowserListObserver>
      browser_list_observation_{this};
  base::ScopedObservation<Host, Host::Observer> host_observation_{this};

  base::WeakPtrFactory<GlicInstanceImpl> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_IMPL_H_
