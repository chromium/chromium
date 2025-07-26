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
class GlicFocusedBrowserManager : public BrowserListObserver,
                                  public views::WidgetObserver,
                                  public GlicWindowController::StateObserver {
 public:
  explicit GlicFocusedBrowserManager(GlicWindowController* window_controller);
  ~GlicFocusedBrowserManager() override;

  GlicFocusedBrowserManager(const GlicFocusedBrowserManager&) = delete;
  GlicFocusedBrowserManager& operator=(const GlicFocusedBrowserManager&) =
      delete;

  // Returns the currently focused browser window, if there is one.
  // This window must be the candidate browser (see below), and also be
  // sufficiently visible to be considered for sharing.
  BrowserWindowInterface* GetFocusedBrowser() const;

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

  // Callback for changes to the focused browser window, or the candidate
  // to be focused.
  using FocusedBrowserChangedCallback =
      base::RepeatingCallback<void(BrowserWindowInterface* candidate,
                                   BrowserWindowInterface* focused)>;
  base::CallbackListSubscription AddFocusedBrowserChangedCallback(
      FocusedBrowserChangedCallback callback);

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
  void PanelStateChanged(const mojom::PanelState&, Browser*) override;

 private:
  struct FocusedBrowserState {
    FocusedBrowserState();
    ~FocusedBrowserState();
    FocusedBrowserState(const FocusedBrowserState& src);
    FocusedBrowserState& operator=(const FocusedBrowserState& src);

    bool IsSame(const FocusedBrowserState& other) const;

    base::WeakPtr<BrowserWindowInterface> candidate_browser;
    base::WeakPtr<BrowserWindowInterface> focused_browser;
  };

  // True if the mutable attributes of `browser` are valid for Glic focus.
  bool IsBrowserStateValid(BrowserWindowInterface* browser_interface);

  void MaybeUpdateFocusedBrowser(bool debounce = false);
  void PerformMaybeUpdateFocusedBrowser();
  FocusedBrowserState ComputeFocusedBrowserState();
  BrowserWindowInterface* ComputeBrowserCandidate();

  void OnBrowserBecameActive(BrowserWindowInterface* browser_interface);
  void OnBrowserBecameInactive(BrowserWindowInterface* browser_interface);
  void OnGlicWindowActivationChanged(bool active);

  raw_ref<GlicWindowController> window_controller_;

  FocusedBrowserState focused_browser_state_;

  base::CallbackListSubscription window_activation_subscription_;
  std::map<BrowserWindowInterface*, std::vector<base::CallbackListSubscription>>
      browser_subscriptions_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  base::OneShotTimer debouncer_;

  base::RepeatingCallbackList<FocusedBrowserChangedCallback::RunType>
      focused_browser_callback_list_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_FOCUSED_BROWSER_MANAGER_H_
