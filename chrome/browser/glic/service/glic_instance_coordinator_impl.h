// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_COORDINATOR_IMPL_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_COORDINATOR_IMPL_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "chrome/browser/glic/common/glic_tab_observer.h"
#include "chrome/browser/glic/common/instance_independent_hotkey_manager.h"
#include "chrome/browser/glic/glic_tab_restore_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/context/glic_sharing_manager.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/glic_invoke_handler.h"
#include "chrome/browser/glic/service/metrics/glic_instance_coordinator_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class Browser;

namespace tabs {
class TabInterface;
}

namespace gfx {
class Point;
}  // namespace gfx

namespace glic {

class GlicActiveInstanceSharingManager;
class ContextualCueingService;
class WebUIContentsContainer;
class GlicWebContentsWarmingPool;
BASE_DECLARE_FEATURE(kGlicHibernateOnMemoryUsage);

BASE_DECLARE_FEATURE(kGlicMaxAwakeInstances);

class GlicInstanceCoordinatorImpl
    : public GlicInstanceCoordinator,
      public GlicInstanceImpl::InstanceCoordinatorDelegate,
      public base::MemoryPressureListener,
      public GlicInstanceCoordinatorMetrics::DataProvider,
      public signin::IdentityManager::Observer {
 public:
  GlicInstanceCoordinatorImpl(const GlicInstanceCoordinatorImpl&) = delete;
  GlicInstanceCoordinatorImpl& operator=(const GlicInstanceCoordinatorImpl&) =
      delete;

  GlicInstanceCoordinatorImpl(
      Profile* profile,
      signin::IdentityManager* identity_manager,
      GlicKeyedService* service,
      GlicEnabling* enabling,
      ContextualCueingService* contextual_cueing_service);
  ~GlicInstanceCoordinatorImpl() override;

  GlicKeyedService* service() { return service_; }

  // GlicInstanceImpl::InstanceCoordinatorDelegate implementation
  void OnInstanceVisibilityChanged(GlicInstanceImpl* instance,
                                   bool is_showing) override;
  void OnInstanceActivationChanged(GlicInstanceImpl* instance,
                                   bool is_active) override;
  void SwitchConversation(
      GlicInstanceImpl& source_instance,
      const ShowOptions& options,
      glic::mojom::ConversationInfoPtr info,
      mojom::WebClientHandler::SwitchConversationCallback callback) override;
  // Closes any existing GlicFloatingUi. This enforces at most one floating ui
  // per profile.
  void OnWillCreateFloaty() override;
  void UnbindTabFromAnyInstance(tabs::TabInterface* tab) override;
  // Sorts conversations by recency and returns the ConversationInfoPtr of each
  // conversation. Used by the web client to get recent conversations.
  std::vector<glic::mojom::ConversationInfoPtr> GetRecentlyActiveConversations(
      size_t limit) override;
  void ContextAccessIndicatorChanged(GlicInstanceImpl& instance,
                                     bool enabled) override;
  std::unique_ptr<WebUIContentsContainer> CreateWebUIContentsContainer()
      override;

  // signin::IdentityManager::Observer implementation
  void OnPrimaryAccountChanged(
      const signin::PrimaryAccountChangeEvent& event_details) override;

  // GlicInstanceCoordinatorMetrics::DataProvider implementation
  int GetVisibleInstanceCount() const override;

  bool IsAnyPanelShowing() const override;
  bool IsConversationPresent(const std::string& conversation_id) const override;
  GlicInstanceCoordinator::ActivateTabResult ActivateTabWithConversation(
      const std::string& conversation_id) override;
  // GlicInstanceCoordinator implementation
  GlicInstance* GetInstanceForTab(const tabs::TabInterface* tab) const override;
  GlicInstance* GetInstanceWithGlicWebContents(
      content::WebContents* glic_web_contents) const override;
  // Sorts instances by recency and returns the instance id and
  // conversation title of each conversation.
  std::vector<ConversationInfo> GetRecentlyActiveInstances(
      size_t limit) override;

  bool IsTabPinnedToAnyInstance(
      const tabs::TabHandle& tab_handle) const override;

  void UnpinTabsFromAllInstances(base::span<const tabs::TabHandle> tab_handles,
                                 GlicUnpinTrigger trigger) override;

  // Creates a new conversation and pins the given tabs.
  // This overrides any conversation that was already associated with any
  // of the given tabs.
  void CreateNewConversationForTabs(
      const std::vector<tabs::TabInterface*>& tabs) override;

  // Pins the given tabs to the instance with the given id.
  void ShowInstanceForTabs(const std::vector<tabs::TabInterface*>& tabs,
                           const InstanceId& instance_id) override;

  // Toggles the side panel for the active tab if `browser` is provided,
  // otherwise toggles the floating window for the instance. Focus is given
  // to the new panel when opening through toggle since it is assumed all toggle
  // sources are user initiated.
  void Toggle(BrowserWindowInterface* browser,
              bool prevent_close,
              mojom::InvocationSource source) override;
  void EnsurePreload() override;
  // Shuts down all hosts. Only call it before destruction of the instance
  // coordinator.
  void Shutdown() override;
  void Close(const CloseOptions& options) override;
  base::WeakPtr<GlicInstance> Invoke(GlicInvokeOptions options) override;
  base::WeakPtr<GlicInstance> InvokeWithAutoSubmit(
      InvokeWithAutoSubmitPasskey auto_submit_passkey,
      GlicInvokeOptions options);
  base::WeakPtr<GlicInstance> InvokeWithAutoSubmit(
      InvokeWithAutoSubmitPasskey auto_submit_passkey,
      GlicInvokeOptions options,
      GlicInvokeWithAutoSubmitOptions auto_submit_options);
  void GetExperimentalTriggeringUpdates(
      mojo::PendingRemote<mojom::ExperimentalTriggeringUpdatesHandler> handler,
      base::OnceCallback<void(bool)> success_status_callback) override;

  void CloseInstanceWithFrame(
      content::RenderFrameHost* render_frame_host) override;
  void CloseAndShutdownInstanceWithFrame(
      content::RenderFrameHost* render_frame_host) override;
  void ArchiveInstanceWithFrame(
      content::RenderFrameHost* render_frame_host) override;

  bool IsDetached() const override;
  bool IsPanelShowingForBrowser(
      const BrowserWindowInterface& bwi) const override;

  base::CallbackListSubscription AddGlobalShowHideCallback(
      base::RepeatingClosure callback) override;
  void Reload(content::RenderFrameHost* render_frame_host) override;
  base::WeakPtr<GlicInstanceCoordinatorImpl> GetWeakPtr();

  base::CallbackListSubscription
  AddActiveInstanceChangedCallbackAndNotifyImmediately(
      ActiveInstanceChangedCallback callback) override;
  GlicInstance* GetActiveInstance() override;
  GlicSharingManager& active_instance_sharing_manager() override;

  // Returns a pointer to an instance with a Floaty embedder or nullptr.
  GlicInstanceImpl* GetInstanceWithFloaty() const;

  // Testing support.
  void SetWarmingEnabledForTesting(bool warming_enabled);
  GlicWebContentsWarmingPool& GetWebContentsWarmingPoolForTesting();
  std::string DescribeForTesting();
  std::vector<GlicInstanceImpl*> GetInstancesForTesting();
  GlicInstanceCoordinatorMetrics& GetMetricsForTesting() { return metrics_; }
  InstanceIndependentHotkeyManager* GetHotkeyManagerForTesting() {
    return hotkey_manager_.get();
  }

  // Testing support. These methods should not be added to the public interface.
  GlicInstanceImpl* GetInstanceImplFor(const InstanceId& id) const;
  GlicInstanceImpl* GetInstanceImplForTab(const tabs::TabInterface* tab) const;

 private:
  void RemoveAllInstances();
  base::WeakPtr<GlicInstance> InvokeInternal(
      std::optional<InvokeWithAutoSubmitPasskey> auto_submit_passkey,
      GlicInvokeOptions options,
      GlicInvokeWithAutoSubmitOptions auto_submit_options);

  void OnTabEvent(const GlicTabEvent& event);
  // Returns a pointer to an instance with the given conversation id or nullptr
  // if no such instance exists.
  GlicInstanceImpl* GetInstanceImplForConversationId(
      const std::string& conversation_id) const;
  GlicInstanceImpl* GetOrCreateInstanceImplForConversationId(
      const std::string& conversation_id,
      const std::optional<std::string>& turn_id);
  GlicInstanceImpl* GetOrCreateGlicInstanceImplForTab(tabs::TabInterface* tab);
  GlicInstanceImpl* GetOrCreateInstanceImplForFloaty();
  GlicInstanceImpl* CreateGlicInstance(
      std::optional<InstanceId> instance_id = std::nullopt);
  std::unique_ptr<GlicInstanceImpl> CreateInstanceImpl(
      std::optional<InstanceId> instance_id = std::nullopt);
  void CreateWarmedInstance();

  // Helper method to get a list of recently active instances sorted by time.
  std::vector<GlicInstanceImpl*> GetSortedRecentInstances(size_t limit) const;

  // GlicInstanceCoordinatorMetrics::DataProvider implementation
  std::vector<InstanceWebContents> GetAllUnhibernatedWebContents() override;

  void ShowInstanceForTabs(GlicInstanceImpl* instance,
                           const std::vector<tabs::TabInterface*>& tabs,
                           GlicPinTrigger pin_trigger);

  void ToggleFloaty(bool prevent_close, glic::mojom::InvocationSource source);
  void ToggleSidePanel(BrowserWindowInterface* browser,
                       bool prevent_close,
                       glic::mojom::InvocationSource source);

  void CloseFloaty(const CloseOptions& options = {});

  void OnMemoryPressure(base::MemoryPressureLevel level) override;
  void ApplyMaxAwakeInstancesLimit();

  void RemoveInstance(InstanceId id) override;

  void NotifyActiveInstanceChanged();
  void ComputeContentAccessIndicator();

  // If a side panel instance becomes active, any separate floaty that is
  // currently listening should stop.
  void MaybeStopListeningFloaty(GlicInstanceImpl* active_instance);

  void OnTabsInserted(const TabStripModelChange::Insert* insert);
  void MaybeDaisyChainNewTab(const TabCreationEvent& event);
  void MaybeDaisyChainFromLinkClick(const TabCreationEvent& event);
  void MaybeDaisyChainFromBookmark(const TabCreationEvent& event);

  void OnInvokeHandlerComplete(GlicInstance* instance,
                               GlicInvokeHandler* handler);

  GlicInstanceImpl* GetOrRestoreInstanceImpl(
      const GlicRestoredState::InstanceInfo& instance_info);
  void RestoreTab(content::WebContents* web_contents,
                  const GlicRestoredState& state);

  // A unique ID for this coordinator, used to generate unique instance IDs.
  const uint64_t coordinator_uid_;

  const raw_ptr<Profile> profile_;
  raw_ptr<GlicKeyedService> service_;
  raw_ptr<ContextualCueingService> contextual_cueing_service_;

  uint32_t next_instance_index_ = 0;
  std::map<InstanceId, std::unique_ptr<GlicInstanceImpl>> instances_;

  base::flat_map<GlicInstance*, std::unique_ptr<GlicInvokeHandler>>
      invoke_handlers_;

  raw_ptr<GlicInstanceImpl> active_instance_ = nullptr;
  raw_ptr<GlicInstanceImpl> last_active_instance_ = nullptr;
  base::RepeatingCallbackList<void(GlicInstance*)>
      active_instance_changed_callback_list_;
  base::RepeatingClosureList global_show_hide_callback_list_;

  base::MemoryPressureListenerRegistration
      memory_pressure_listener_registration_;

  bool warming_enabled_ = true;

  GlicInstanceCoordinatorMetrics metrics_;

  std::unique_ptr<GlicTabObserver> tab_observer_;
  std::unique_ptr<GlicWebContentsWarmingPool> web_contents_warming_pool_;

  base::ScopedObservation<signin::IdentityManager,
                          signin::IdentityManager::Observer>
      identity_manager_observation_{this};

  std::unique_ptr<InstanceIndependentHotkeyManager> hotkey_manager_;
  std::unique_ptr<GlicActiveInstanceSharingManager>
      active_instance_sharing_manager_;
  base::WeakPtrFactory<GlicInstanceCoordinatorImpl> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_COORDINATOR_IMPL_H_
