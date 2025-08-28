// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_WIDGET_GLIC_PANEL_COORDINATOR_IMPL_H_
#define CHROME_BROWSER_GLIC_WIDGET_GLIC_PANEL_COORDINATOR_IMPL_H_

#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/scoped_observation_traits.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/host/glic_web_client_access.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class Browser;

namespace gfx {
class Size;
class Point;
}  // namespace gfx
namespace glic {
class GlicPanelCoordinatorImpl : public GlicWindowController {
 public:
  GlicPanelCoordinatorImpl(const GlicPanelCoordinatorImpl&) = delete;
  GlicPanelCoordinatorImpl& operator=(const GlicPanelCoordinatorImpl&) = delete;

  GlicPanelCoordinatorImpl(Profile* profile,
                           signin::IdentityManager* identity_manager,
                           GlicKeyedService* service,
                           GlicEnabling* enabling);
  ~GlicPanelCoordinatorImpl() override;

  // GlicWindowController implementation
  void Toggle(BrowserWindowInterface* browser,
              bool prevent_close,
              mojom::InvocationSource source) override;
  bool ActivateBrowser() override;
  void ShowAfterSignIn(base::WeakPtr<Browser> browser) override;
  void ToggleWhenNotAlwaysDetached(Browser* new_attached_browser,
                                   bool prevent_close,
                                   mojom::InvocationSource source) override;
  void FocusIfOpen() override;
  void Attach() override;
  void Detach() override;
  void Shutdown() override;
  void Resize(const gfx::Size& size,
              base::TimeDelta duration,
              base::OnceClosure callback) override;
  void EnableDragResize(bool enabled) override;
  void MaybeSetWidgetCanResize() override;
  gfx::Size GetSize() override;
  void SetDraggableAreas(
      const std::vector<gfx::Rect>& draggable_areas) override;
  void SetMinimumWidgetSize(const gfx::Size& size) override;
  void Close() override;
  void CloseWithReason(views::Widget::ClosedReason reason) override;
  void ShowTitleBarContextMenuAt(gfx::Point event_loc) override;
  bool ShouldStartDrag(const gfx::Point& initial_press_loc,
                       const gfx::Point& mouse_location) override;
  const mojom::PanelState& GetPanelState() const override;

  void AddStateObserver(StateObserver* observer) override;
  void RemoveStateObserver(StateObserver* observer) override;

  bool IsActive() override;
  bool IsShowing() const override;
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
  std::unique_ptr<GlicView> CreateGlicViewForSidePanel() override;

  base::CallbackListSubscription RegisterFloatyStateChange(
      FloatyStateChangeCallback callback) override;

 private:
  // List of callbacks to be notified when window activation has changed.
  base::RepeatingCallbackList<void(bool)> window_activation_callback_list_;

  using FloatyStateChangeCallbackList =
      base::RepeatingCallbackList<void(State, mojom::CurrentView view)>;
  FloatyStateChangeCallbackList floaty_state_change_callback_list_;

  mojom::PanelState panel_state_;
  const raw_ptr<Profile> profile_;
};
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_WIDGET_GLIC_PANEL_COORDINATOR_IMPL_H_
