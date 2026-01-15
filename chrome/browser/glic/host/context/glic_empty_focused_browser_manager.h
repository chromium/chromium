// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_EMPTY_FOCUSED_BROWSER_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_EMPTY_FOCUSED_BROWSER_MANAGER_H_

#include "build/build_config.h"
#include "chrome/browser/glic/host/context/glic_focused_browser_manager.h"

class BrowserWindowInterface;

namespace glic {

// Simple focused browser manager implementation for single browser strategies
// where the browser window can never go out of focus.
class GlicEmptyFocusedBrowserManager : public GlicFocusedBrowserManager {
 public:
  GlicEmptyFocusedBrowserManager();
  ~GlicEmptyFocusedBrowserManager() override;
  GlicEmptyFocusedBrowserManager(const GlicEmptyFocusedBrowserManager&) =
      delete;
  GlicEmptyFocusedBrowserManager& operator=(
      const GlicEmptyFocusedBrowserManager&) = delete;

  // GlicFocusedBrowserManager implementation.
  using FocusedBrowserChangedCallback =
      base::RepeatingCallback<void(BrowserWindowInterface* candidate,
                                   BrowserWindowInterface* focused)>;
  base::CallbackListSubscription AddFocusedBrowserChangedCallback(
      FocusedBrowserChangedCallback callback) override;
  base::CallbackListSubscription AddActiveBrowserChangedCallback(
      base::RepeatingCallback<void(BrowserWindowInterface*)> callback) override;
  BrowserWindowInterface* GetFocusedBrowser() const override;
  BrowserWindowInterface* GetActiveBrowser() const override;
  BrowserWindowInterface* GetCandidateBrowser() const override;
  void OnGlicWindowActivationChanged(bool active) override {}
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_EMPTY_FOCUSED_BROWSER_MANAGER_H_
