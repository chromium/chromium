// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_PUBLIC_WIDGET_GLIC_SIDE_PANEL_COORDINATOR_DESKTOP_ANDROID_H_
#define CHROME_BROWSER_GLIC_PUBLIC_WIDGET_GLIC_SIDE_PANEL_COORDINATOR_DESKTOP_ANDROID_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_side_panel_coordinator.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/side_panel/side_panel_native_view.h"

class SidePanelEntryScope;
class SidePanelRegistry;
class SidePanelUI;

namespace glic {
class CoBrowseViewsBridge;
class GlicKeyedService;

// GlicSidePanelCoordinatorDesktopAndroid handles the creation and registration
// of the glic SidePanelEntry for Android Desktop.
class GlicSidePanelCoordinatorDesktopAndroid : public GlicSidePanelCoordinator,
                                               public SidePanelEntryObserver {
 public:
  GlicSidePanelCoordinatorDesktopAndroid(tabs::TabInterface* tab,
                                         SidePanelRegistry* side_panel_registry,
                                         Profile* profile);
  ~GlicSidePanelCoordinatorDesktopAndroid() override;

  // GlicSidePanelCoordinator:
  using GlicSidePanelCoordinator::Close;
  using GlicSidePanelCoordinator::Show;
  void Show(const ShowOptions& options) override;
  void Close(const CloseOptions& options) override;
  bool IsShowing() const override;
  State state() override;
  bool SupportsPeek() const override;
  base::CallbackListSubscription AddStateCallback(
      base::RepeatingCallback<void(State state)> callback) override;
  void SetWebContents(content::WebContents* web_contents) override;
  int GetPreferredWidth() override;
  bool IsGlicSidePanelActive() override;

  // Called when the Glic enabled status changes for `profile_`.
  void OnGlicEnabledChanged();

 protected:
  // SidePanelEntryObserver:
  void OnEntryHiddenWithReason(SidePanelEntry* entry,
                               SidePanelEntryHideReason reason) override;
  void OnEntryShown(SidePanelEntry* entry) override;

 private:
  void CheckStateAfterHidden();

  // Create and register the Glic side panel entry.
  void CreateAndRegisterEntry();

  // Returns the SidePanelCoordinator for the window associated with `tab_`.
  SidePanelUI* GetWindowSidePanelUI() const;

  // Gets the Glic WebView from the Glic service.
  SidePanelNativeView CreateView(SidePanelEntryScope& scope);

  // Sets a new state and notifies about a state change.
  void SetState(State new_state);

  raw_ptr<tabs::TabInterface> tab_ = nullptr;
  raw_ptr<SidePanelRegistry> side_panel_registry_ = nullptr;
  base::WeakPtr<SidePanelEntry> entry_;
  base::CallbackListSubscription on_glic_enabled_changed_subscription_;
  base::RepeatingCallbackList<void(State state)> state_changed_callbacks_;

  State state_ = State::kClosed;

  std::unique_ptr<CoBrowseViewsBridge> cobrowse_views_bridge_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  raw_ptr<GlicKeyedService> glic_service_ = nullptr;

  base::WeakPtrFactory<GlicSidePanelCoordinatorDesktopAndroid>
      weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_PUBLIC_WIDGET_GLIC_SIDE_PANEL_COORDINATOR_DESKTOP_ANDROID_H_
