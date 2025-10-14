// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_COORDINATOR_IMPL_H_
#define CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_COORDINATOR_IMPL_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
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

namespace glic {

// An interface to GlicInstanceCoordinatorImpl. Should be used instead of direct
// access to GlicInstanceCoordinatorImpl to allow for test fakes.
class GlicInstanceCoordinator
    : public GlicWindowController,
      public GlicInstanceImpl::InstanceCoordinatorDelegate {};

class GlicInstanceCoordinatorImpl : public GlicInstanceCoordinator {
 public:
  GlicInstanceCoordinatorImpl(const GlicInstanceCoordinatorImpl&) = delete;
  GlicInstanceCoordinatorImpl& operator=(const GlicInstanceCoordinatorImpl&) =
      delete;

  GlicInstanceCoordinatorImpl(Profile* profile,
                              signin::IdentityManager* identity_manager,
                              GlicKeyedService* service,
                              GlicEnabling* enabling);
  ~GlicInstanceCoordinatorImpl() override;

  // GlicInstanceImpl::InstanceCoordinatorDelegate implementation
  void OnInstanceVisibilityChanged(GlicInstance* instance,
                                   bool is_showing) override;
  void SwitchConversation(
      GlicInstanceImpl& source_instance,
      const ShowOptions& options,
      glic::mojom::ConversationInfoPtr info,
      mojom::WebClientHandler::SwitchConversationCallback callback) override;
  void OnDetachRequested(GlicInstance* instance,
                         tabs::TabInterface* tab) override;

  // GlicWindowController implementation
  HostManager& host_manager() override;
  std::vector<GlicInstance*> GetInstances() override;
  GlicInstance* GetInstanceForTab(tabs::TabInterface* tab) override;

  void Toggle(BrowserWindowInterface* browser,
              bool prevent_close,
              mojom::InvocationSource source) override;
  bool ActivateBrowser() override;
  void ShowAfterSignIn(base::WeakPtr<Browser> browser) override;
  void FocusIfOpen() override;
  void Shutdown() override;
  void MaybeSetWidgetCanResize() override;
  void Close() override;
  void ShowTitleBarContextMenuAt(gfx::Point event_loc) override;

  void AddGlobalStateObserver(StateObserver* observer) override;
  void RemoveGlobalStateObserver(StateObserver* observer) override;
  mojom::PanelState GetGlobalPanelState() override;

  bool IsActive() override;
  bool IsDetached() const override;
  base::CallbackListSubscription AddWindowActivationChangedCallback(
      WindowActivationChangedCallback callback) override;
  void Preload() override;
  void Reload(content::RenderFrameHost* render_frame_host) override;
  base::WeakPtr<GlicInstanceCoordinatorImpl> GetWeakPtr();

  base::WeakPtr<views::View> GetGlicViewAsView() override;
  GlicWidget* GetGlicWidget() const override;
  gfx::NativeWindow GetHostNativeWindow() override;

  Browser* attached_browser() override;
  State state() const override;
  Profile* profile() override;
  gfx::Rect GetInitialBounds(Browser* browser) override;
  void ShowDetachedForTesting() override;
  void SetPreviousPositionForTesting(gfx::Point position) override;

  base::CallbackListSubscription RegisterLastActiveInstanceChangedCallback(
      LastActiveInstanceChangedCallback callback) override;

  void FindInstanceFromGlicContentsAndBindToTab(
      content::WebContents* source_glic_web_contents,
      tabs::TabInterface* tab_to_bind) override;

  // Testing support.
  void SetWarmingEnabledForTesting(bool warming_enabled);

 private:
  GlicInstanceImpl* GetOrCreateGlicInstanceImplForTab(tabs::TabInterface* tab);
  GlicInstanceImpl* GetInstanceImplFor(const InstanceId& id);
  GlicInstanceImpl* GetInstanceImplForTab(tabs::TabInterface* tab);
  GlicInstanceImpl* CreateGlicInstance();
  void CreateWarmedInstance();

  void ToggleFloaty(bool prevent_close);
  void ToggleSidePanel(BrowserWindowInterface* browser, bool prevent_close);

  void RemoveInstance(GlicInstance* instance) override;
  bool HasAttachedInstance(GlicInstance* instance);

  void NotifyLastActiveInstanceChanged();

  // Returns a pointer to an instance with a Floaty embedder or nullptr.
  GlicInstanceImpl* GetInstanceWithFloaty();

  // List of callbacks to be notified when window activation has changed.
  base::RepeatingCallbackList<void(bool)> window_activation_callback_list_;

  mojom::PanelState panel_state_;
  const raw_ptr<Profile> profile_;

  std::map<InstanceId, std::unique_ptr<GlicInstanceImpl>> instances_;

  // The instance ID of the one instance that is currently floating.
  std::optional<InstanceId> floating_instance_key_;

  std::unique_ptr<GlicInstanceImpl> warmed_instance_;

  std::unique_ptr<HostManager> host_manager_;

  raw_ptr<GlicInstance> last_active_instance_ = nullptr;
  base::RepeatingCallbackList<void(GlicInstance*)>
      last_active_instance_changed_callback_list_;

  bool warming_enabled_ = true;

  base::WeakPtrFactory<GlicInstanceCoordinatorImpl> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_COORDINATOR_IMPL_H_
