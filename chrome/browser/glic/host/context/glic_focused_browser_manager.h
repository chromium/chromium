// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_FOCUSED_BROWSER_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_FOCUSED_BROWSER_MANAGER_H_

#include <map>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/glic/host/context/glic_focused_browser_manager_interface.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/ui/browser_list_observer.h"
#include "ui/views/widget/widget_observer.h"

class Browser;
class BrowserWindowInterface;

namespace views {
class Widget;
}  // namespace views

namespace glic {

// Responsible for managing which browser window is considered "focused".
class GlicFocusedBrowserManager : public GlicFocusedBrowserManagerInterface,
                                  public BrowserListObserver,
                                  public views::WidgetObserver,
                                  public GlicWindowController::StateObserver {
 public:
  explicit GlicFocusedBrowserManager(
      GlicInstance::UIDelegate* window_controller,
      Profile* profile);
  ~GlicFocusedBrowserManager() override;

  GlicFocusedBrowserManager(const GlicFocusedBrowserManager&) = delete;
  GlicFocusedBrowserManager& operator=(const GlicFocusedBrowserManager&) =
      delete;

  // GlicFocusedBrowserInterface implementation.
  using FocusedBrowserChangedCallback =
      base::RepeatingCallback<void(BrowserWindowInterface* candidate,
                                   BrowserWindowInterface* focused)>;
  base::CallbackListSubscription AddFocusedBrowserChangedCallback(
      FocusedBrowserChangedCallback callback) override;
  base::CallbackListSubscription AddActiveBrowserChangedCallback(
      base::RepeatingCallback<void(BrowserWindowInterface*)> callback) override;
  BrowserWindowInterface* GetFocusedBrowser() const override;
  BrowserWindowInterface* GetActiveBrowser() const override;
  void OnGlicWindowActivationChanged(bool active) override;

  // Returns the candidate for the focused browser window, if there is one.
  // This browser must not be one that will never be shareable (see
  // `IsBrowserValidForSharing`), and it must be either the currently focused
  // window, or the most recently focused window if the Glic panel is focused
  // instead.
  //
  // This is separately exposed so that the UI state can reflect that a
  // particular tab isn't shared because the most recently focused window isn't
  // visible.
  BrowserWindowInterface* GetCandidateBrowser() const;

  // BrowserListObserver
  void OnBrowserAdded(Browser* browser) override;
  void OnBrowserRemoved(Browser* browser) override;

  // views::WidgetObserver
  void OnWidgetShowStateChanged(views::Widget* widget) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;
  void OnWidgetVisibilityOnScreenChanged(views::Widget* widget,
                                         bool visible) override;
  void OnWidgetDestroyed(views::Widget* widget) override;

  // GlicWindowController::StateObserver:
  void PanelStateChanged(
      const mojom::PanelState&,
      const GlicWindowController::PanelStateContext& context) override;

  // Sets whether the manager is in testing mode. When in testing mode, logic
  // for determining the active browser is modified to be more deterministic.
  static void SetTestingModeForTesting(bool testing_mode);

 private:
  // Tracks the state of the focused browser and candidate focused browser.
  struct FocusedBrowserState {
    FocusedBrowserState();
    ~FocusedBrowserState();
    FocusedBrowserState(const FocusedBrowserState& src);
    FocusedBrowserState& operator=(const FocusedBrowserState& src);

    bool IsSame(const FocusedBrowserState& other) const;

    base::WeakPtr<BrowserWindowInterface> candidate_browser;
    base::WeakPtr<BrowserWindowInterface> focused_browser;
  };

  // Browser state tracked by this manager.
  struct BrowserState {
    BrowserState();
    ~BrowserState();
    BrowserState(const BrowserState& src);
    BrowserState& operator=(const BrowserState& src);

    FocusedBrowserState focused_state;
    // The active, but not necessarily focused, browser.
    base::WeakPtr<BrowserWindowInterface> active_browser;
  };

  // True if the mutable attributes of `browser` are valid for Glic focus.
  bool IsBrowserStateValid(BrowserWindowInterface* browser_interface);

  void MaybeUpdateFocusedBrowser(bool debounce = false);
  void PerformMaybeUpdateFocusedBrowser();
  FocusedBrowserState ComputeFocusedBrowserState();
  BrowserWindowInterface* ComputeBrowserCandidate();
  BrowserWindowInterface* ComputeActiveBrowser();
  BrowserState ComputeBrowserState();

  void OnBrowserBecameActive(BrowserWindowInterface* browser_interface);
  void OnBrowserBecameInactive(BrowserWindowInterface* browser_interface);

  void Initialize();

  bool is_initialized_ = false;

  raw_ref<GlicInstance::UIDelegate> window_controller_;

  BrowserState browser_state_;

  base::CallbackListSubscription window_activation_subscription_;
  std::map<BrowserWindowInterface*, std::vector<base::CallbackListSubscription>>
      browser_subscriptions_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  base::OneShotTimer debouncer_;

  base::RepeatingCallbackList<FocusedBrowserChangedCallback::RunType>
      focused_browser_callback_list_;
  base::RepeatingCallbackList<void(BrowserWindowInterface*)>
      active_browser_callback_list_;

  raw_ptr<Profile> profile_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_FOCUSED_BROWSER_MANAGER_H_
