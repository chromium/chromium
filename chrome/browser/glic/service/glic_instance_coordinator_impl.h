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
class Size;
class Point;
}  // namespace gfx

namespace glic {
class GlicInstanceCoordinatorImpl
    : public GlicWindowController,
      public GlicInstanceImpl::AttachmentDelegate {
 public:
  GlicInstanceCoordinatorImpl(const GlicInstanceCoordinatorImpl&) = delete;
  GlicInstanceCoordinatorImpl& operator=(const GlicInstanceCoordinatorImpl&) =
      delete;

  GlicInstanceCoordinatorImpl(Profile* profile,
                              signin::IdentityManager* identity_manager,
                              GlicKeyedService* service,
                              GlicEnabling* enabling);
  ~GlicInstanceCoordinatorImpl() override;

  // GlicInstanceImpl::AttachmentDelegate implementation
  void AttachInstance(GlicInstance* instance) override;
  void DetachInstance(GlicInstance* instance) override;
  void OnInstanceOrphaned(GlicInstance* instance) override;
  void SwitchConversation(
      tabs::TabInterface* tab,
      const std::string& conversation_id,
      mojom::WebClientHandler::SwitchConversationCallback callback) override;

  // GlicWindowController implementation
  HostManager& host_manager() override;
  std::vector<Host*> GetHosts() override;
  Host* GetHostForTab(tabs::TabInterface* tab) override;
  std::vector<GlicInstance*> GetInstances() override;
  GlicInstance* GetInstanceForTab(tabs::TabInterface* tab) override;

  void Toggle(BrowserWindowInterface* browser,
              bool prevent_close,
              mojom::InvocationSource source) override;
  bool ActivateBrowser() override;
  void ShowAfterSignIn(base::WeakPtr<Browser> browser) override;
  void ToggleWhenNotAlwaysDetached(Browser* new_attached_browser,
                                   bool prevent_close,
                                   mojom::InvocationSource source) override;
  void FocusIfOpen() override;
  void Shutdown() override;
  void MaybeSetWidgetCanResize() override;
  gfx::Size GetSize() override;
  void Close() override;
  void CloseWithReason(views::Widget::ClosedReason reason) override;
  void ShowTitleBarContextMenuAt(gfx::Point event_loc) override;
  bool ShouldStartDrag(const gfx::Point& initial_press_loc,
                       const gfx::Point& mouse_location) override;

  void AddStateObserver(StateObserver* observer) override;
  void RemoveStateObserver(StateObserver* observer) override;

  const mojom::PanelState& GetPanelState() const override;
  bool IsShowing() const override;

  bool IsActive() override;
  bool IsAttached() const override;
  bool IsDetached() const override;
  base::CallbackListSubscription AddWindowActivationChangedCallback(
      WindowActivationChangedCallback callback) override;
  void Preload() override;
  void Reload() override;
  bool IsWarmed() const override;
  base::WeakPtr<GlicWindowController> GetWeakPtr() override;

  GlicView* GetGlicView() const override;
  base::WeakPtr<views::View> GetGlicViewAsView() override;
  GlicWidget* GetGlicWidget() const override;
  gfx::NativeWindow GetHostNativeWindow() override;

  Browser* attached_browser() override;
  State state() const override;
  GlicWindowAnimator* window_animator() override;
  Profile* profile() override;
  gfx::Rect GetInitialBounds(Browser* browser) override;
  void ShowDetachedForTesting() override;
  void SetPreviousPositionForTesting(gfx::Point position) override;
  std::unique_ptr<views::View> CreateViewForSidePanel(
      tabs::TabInterface& tab) override;
  void SidePanelShown(BrowserWindowInterface* browser) override;

  base::CallbackListSubscription RegisterFloatyStateChange(
      FloatyStateChangeCallback callback) override;

 private:
  GlicInstanceImpl* GetOrCreateGlicInstanceImplForTab(tabs::TabInterface* tab);
  GlicInstanceImpl* GetInstanceImplFor(const InstanceId& id);
  GlicInstanceImpl* GetInstanceImplForTab(tabs::TabInterface* tab);
  GlicInstanceImpl* CreateGlicInstance();

  void ToggleFloaty();
  void ToggleSidePanel(BrowserWindowInterface* browser);

  void RemoveInstance(GlicInstance* instance);
  bool HasAttachedInstance(GlicInstance* instance);

  // List of callbacks to be notified when window activation has changed.
  base::RepeatingCallbackList<void(bool)> window_activation_callback_list_;
  using FloatyStateChangeCallbackList =
      base::RepeatingCallbackList<void(State, mojom::CurrentView view)>;
  FloatyStateChangeCallbackList floaty_state_change_callback_list_;

  mojom::PanelState panel_state_;
  const raw_ptr<Profile> profile_;

  std::map<InstanceId, std::unique_ptr<GlicInstanceImpl>> instances_;

  // The instance ID of the one instance that is currently floating.
  std::optional<InstanceId> floating_instance_key_;

  std::unique_ptr<HostManager> host_manager_;

  base::WeakPtrFactory<GlicInstanceCoordinatorImpl> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_SERVICE_GLIC_INSTANCE_COORDINATOR_IMPL_H_
