// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_PAGE_STABILITY_WAITER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_PAGE_STABILITY_WAITER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/actor.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebContents;
}

// Waits for page stability by delegating to the renderer's
// `actor::mojom::PageStabilityMonitor`.
class PasswordChangePageStabilityWaiter {
 public:
  // Starts monitoring for page stability on the current primary main frame in
  // `web_contents` and invokes `callback` once the page reaches a stable state.
  PasswordChangePageStabilityWaiter(content::WebContents* web_contents,
                                    base::OnceClosure callback);
  ~PasswordChangePageStabilityWaiter();

 private:
  void OnStable();
  void OnDisconnect();

  base::OnceClosure callback_;
  mojo::Remote<actor::mojom::PageStabilityMonitor> monitor_;
  base::WeakPtrFactory<PasswordChangePageStabilityWaiter> weak_ptr_factory_{
      this};
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_CHANGE_PASSWORD_CHANGE_PAGE_STABILITY_WAITER_H_
