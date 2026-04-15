// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_PAGE_STABILITY_WAITER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_PAGE_STABILITY_WAITER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/common/actor.mojom.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace password_manager {
class PasswordManagerClient;
}

namespace content {
class NavigationHandle;
class WebContents;
}

// Waits for page stability before proceeding with password change operations.
// Analysis has shown that many sites perform extensive DOM modifications and
// layout shifts during page load, which can interfere with form detection and
// interaction.
//
// This class ensures page stability by executing a renderer stability check
// followed by a visual state check:
// 1. **Renderer Stability**: It binds to the
// `actor::mojom::PageStabilityMonitor`
//    Mojo interface in the renderer to wait for initial paint stability.
// 2. **Compositor Visual State**: It inserts a visual state callback to ensure
//    the browser has received a frame from the renderer's compositor.

// It also observes `DidFinishNavigation` to restart the monitoring sequence
// if the page is reloaded or navigated during the wait.
// A safeguard timeout is used to ensure the completion callback
// is invoked regardless of whether all stability conditions were met.
class PasswordChangePageStabilityWaiter : public content::WebContentsObserver {
 public:
  PasswordChangePageStabilityWaiter(
      content::WebContents* web_contents,
      password_manager::PasswordManagerClient* client,
      base::OnceClosure callback);
  ~PasswordChangePageStabilityWaiter() override;

  // content::WebContentsObserver:
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidStopLoading() override;

 private:
  // Checks if the page is currently loading. If it is, returns early and waits
  // for DidStopLoading(). Otherwise, calls CheckPageStability().
  void StartWaiting();

  void CheckPageStability();
  void CheckVisualState();

  void OnAllChecksCompleted();
  void OnTimeout();

  base::OnceClosure callback_;
  raw_ptr<password_manager::PasswordManagerClient> client_;
  mojo::Remote<actor::mojom::PageStabilityMonitor> monitor_;
  base::OneShotTimer timeout_timer_;
  base::WeakPtrFactory<PasswordChangePageStabilityWaiter> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_PAGE_STABILITY_WAITER_H_
