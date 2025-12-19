// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_COORDINATOR_IMPL_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_COORDINATOR_IMPL_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/service/glic_tab_creation_observer.h"
#include "chrome/browser/glic/service/metrics/glic_instance_coordinator_metrics.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class Browser;

namespace tabs {
class TabInterface;
}

namespace gfx {
class Point;
}  // namespace gfx

namespace contextual_cueing {
class ContextualCueingService;
}
namespace glic {

BASE_FEATURE(kGlicHibernateAllOnMemoryPressure,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_DECLARE_FEATURE(kGlicHibernateOnMemoryUsage);

// An interface to GlicInstanceCoordinatorImpl. Should be used instead of direct
// access to GlicInstanceCoordinatorImpl to allow for test fakes.
class GlicInstanceCoordinator : public GlicWindowController {};

class GlicInstanceCoordinatorImpl
    : public GlicInstanceCoordinator,
      public GlicInstanceImpl::InstanceCoordinatorDelegate,
      public base::MemoryPressureListener,
      public GlicInstanceCoordinatorMetrics::DataProvider {
 public:
  GlicInstanceCoordinatorImpl(const GlicInstanceCoordinatorImpl&) = delete;
  GlicInstanceCoordinatorImpl& operator=(const GlicInstanceCoordinatorImpl&) =
      delete;

  GlicInstanceCoordinatorImpl(
      Profile* profile,
      signin::IdentityManager* identity_manager,
      GlicKeyedService* service,
      GlicEnabling* enabling,
      contextual_cueing::ContextualCueingService* contextual_cueing_service);
  ~GlicInstanceCoordinatorImpl() override;

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
  std::vector<glic::mojom::ConversationInfoPtr> GetRecentlyActiveConversations()
      override;
  void ContextAccessIndicatorChanged(GlicInstanceImpl& instance,
                                     bool enabled) override;

  // GlicWindowController and GlicInstanceCoordinatorMetrics::DataProvider implementation
  std::vector<GlicInstance*> GetInstances() override;
  // GlicWindowController implementation
  HostManager& host_manager() override;
  GlicInstance* GetInstanceForTab(const tabs::TabInterface* tab) const override;

  // Toggles the side panel for the active tab if `browser` is provided,
  // otherwise toggles the floating window for the instance. Focus is given
  // to the new panel when opening through toggle since it is assumed all toggle
  // sources are user initiated.
  void Toggle(BrowserWindowInterface* browser,
              bool prevent_close,
              mojom::InvocationSource source,
              std::optional<std::string> prompt_suggestion) override;
  void ShowAfterSignIn(base::WeakPtr<Browser> browser) override;
  // Shuts down all hosts. Only call it before destruction of the instance
  // coordinator.
  void Shutdown() override;
  void Close() override;
  void CloseInstanceWithFrame(
      content::RenderFrameHost* render_frame_host) override;
  void CloseAndShutdownInstanceWithFrame(
      content::RenderFrameHost* render_frame_host) override;

  void AddGlobalStateObserver(StateObserver* observer) override;
  void RemoveGlobalStateObserver(StateObserver* observer) override;

  bool IsDetached() const override;
  bool IsPanelShowingForBrowser(
      const BrowserWindowInterface& bwi) const override;
  base::CallbackListSubscription AddWindowActivationChangedCallback(
      WindowActivationChangedCallback callback) override;
  base::CallbackListSubscription AddGlobalShowHideCallback(
      base::RepeatingClosure callback) override;
  void Preload() override;
  void Reload(content::RenderFrameHost* render_frame_host) override;
  base::WeakPtr<GlicInstanceCoordinatorImpl> GetWeakPtr();

  GlicWidget* GetGlicWidget() const override;

  Browser* attached_browser() override;
  State state() const override;
  Profile* profile() override;
  gfx::Rect GetInitialBounds(Browser* browser) override;
  void ShowDetachedForTesting() override;
  void SetPreviousPositionForTesting(gfx::Point position) override;

  base::CallbackListSubscription
  AddActiveInstanceChangedCallbackAndNotifyImmediately(
      ActiveInstanceChangedCallback callback) override;
  GlicInstance* GetActiveInstance() override;

  // Returns a pointer to an instance with a Floaty embedder or nullptr.
  GlicInstanceImpl* GetInstanceWithFloaty() const;

  // Testing support.
  void SetWarmingEnabledForTesting(bool warming_enabled);
  bool HasWarmedInstanceForTesting() const {
    return warmed_instance_ != nullptr;
  }
  GlicInstanceImpl* GetWarmedInstanceForTesting() {
    return warmed_instance_.get();
  }
  std::string DescribeForTesting();

 private:
  void OnTabCreated(tabs::TabInterface& old_tab, tabs::TabInterface& new_tab);
  GlicInstanceImpl* GetOrCreateGlicInstanceImplForTab(tabs::TabInterface* tab);
  GlicInstanceImpl* GetInstanceImplFor(const InstanceId& id) const;
  GlicInstanceImpl* GetInstanceImplForTab(const tabs::TabInterface* tab) const;
  GlicInstanceImpl* GetOrCreateInstanceImplForFloaty();
  GlicInstanceImpl* CreateGlicInstance();
  void CreateWarmedInstance();

  void ToggleFloaty(bool prevent_close, glic::mojom::InvocationSource source);
  void ToggleSidePanel(BrowserWindowInterface* browser,
                       bool prevent_close,
                       glic::mojom::InvocationSource source);

  void CloseFloaty();

  void OnMemoryPressure(base::MemoryPressureLevel level) override;
  void CheckMemoryUsage();

  void RemoveInstance(GlicInstanceImpl* instance) override;

  void NotifyActiveInstanceChanged();
  void ComputeContentAccessIndicator();

  // List of callbacks to be notified when window activation has changed.
  base::RepeatingCallbackList<void(bool)> window_activation_callback_list_;

  const raw_ptr<Profile> profile_;
  raw_ptr<GlicKeyedService> service_;
  raw_ptr<contextual_cueing::ContextualCueingService>
      contextual_cueing_service_;

  std::map<InstanceId, std::unique_ptr<GlicInstanceImpl>> instances_;

  std::unique_ptr<GlicInstanceImpl> warmed_instance_;

  std::unique_ptr<HostManager> host_manager_;

  raw_ptr<GlicInstanceImpl> active_instance_ = nullptr;
  raw_ptr<GlicInstanceImpl> last_active_instance_ = nullptr;
  base::RepeatingCallbackList<void(GlicInstance*)>
      active_instance_changed_callback_list_;
  base::RepeatingClosureList global_show_hide_callback_list_;

  base::MemoryPressureListenerRegistration
      memory_pressure_listener_registration_;
  base::RepeatingTimer memory_monitor_timer_;

  bool warming_enabled_ = true;

  GlicInstanceCoordinatorMetrics metrics_;

  std::unique_ptr<GlicTabCreationObserver> tab_creation_observer_;

  base::WeakPtrFactory<GlicInstanceCoordinatorImpl> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_COORDINATOR_IMPL_H_
