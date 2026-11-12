// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_WIDGET_GLIC_SIDE_PANEL_COORDINATOR_ANDROID_H_
#define CHROME_BROWSER_GLIC_PUBLIC_WIDGET_GLIC_SIDE_PANEL_COORDINATOR_ANDROID_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/tab_bottom_sheet_bridge.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "components/tabs/public/tab_interface.h"

class BrowserWindowInterface;

namespace ui {
class WindowAndroid;
}

namespace glic {

class GlicCoBrowseViews;

class GlicSidePanelCoordinatorAndroid
    : public GlicSidePanelCoordinator,
      public context_sharing::TabBottomSheetBridge::Observer {
 public:
  explicit GlicSidePanelCoordinatorAndroid(tabs::TabInterface* tab);
  ~GlicSidePanelCoordinatorAndroid() override;

  // GlicSidePanelCoordinator:
  using GlicSidePanelCoordinator::Show;
  void Show(const ShowOptions& options) override;
  void SetCoBrowseViews(
      std::unique_ptr<GlicCoBrowseViews> co_browse_views) override;
  GlicCoBrowseViews* GetCoBrowseViews() const override;
  void Close(const CloseOptions& options) override;
  bool IsShowing() const override;
  State state() override;
  bool SupportsPeek() const override;
  base::CallbackListSubscription AddStateCallback(
      base::RepeatingCallback<void(State state)> callback) override;
  int GetPreferredWidth() override;
  bool IsGlicSidePanelActive() override;

  void SuppressBottomSheetForTesting(bool suppress);
  bool GetRestoreExpandedOnRecreationForTesting() const;
  bool HasBottomSheetBridgeForTesting() const;

  // context_sharing::TabBottomSheetBridge::Observer:
  void OnClosed() override;
  void OnSuppressed() override;
  void OnOpened(bool is_expanded) override;

 private:
  void SetState(State state);
  void SaveStateBeforeDeactivation();
  void OnTabDidActivate(tabs::TabInterface* tab);
  void OnTabWillDeactivate(tabs::TabInterface* tab);
  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason detach_reason);
  // Callback triggered when a TabBottomSheetManager is initialized for a
  // WindowAndroid.
  void OnManagerInitialized(ui::WindowAndroid* window);
  void EnsureBottomSheetBridgeCreated();

  State state_ = State::kClosed;
  // True if the bottom sheet was expanded when the tab began deactivating or
  // detaching. Used to upgrade the initial state back to expanded when
  // TabBottomSheetManager is re-initialized during activity recreation/restart.
  bool restore_expanded_on_recreation_ = false;
  // Non-null if Glic requested to show while waiting for the Java bottom sheet
  // manager layout initialization.
  std::optional<ShowOptions> pending_show_options_;
  base::RepeatingCallbackList<void(State)> state_callbacks_;
  const raw_ref<tabs::TabInterface> tab_;
  std::unique_ptr<GlicCoBrowseViews> co_browse_views_;
  base::CallbackListSubscription did_activate_subscription_;
  base::CallbackListSubscription will_deactivate_subscription_;
  base::CallbackListSubscription will_detach_subscription_;
  base::CallbackListSubscription manager_initialized_subscription_;
  std::unique_ptr<context_sharing::TabBottomSheetBridge>
      tab_bottom_sheet_bridge_;

  base::WeakPtrFactory<GlicSidePanelCoordinatorAndroid> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_WIDGET_GLIC_SIDE_PANEL_COORDINATOR_ANDROID_H_
