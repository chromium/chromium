// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_EMPTY_FOCUSED_BROWSER_MANAGER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_EMPTY_FOCUSED_BROWSER_MANAGER_H_

#include "chrome/browser/glic/host/context/glic_focused_browser_manager_interface.h"
#include "chrome/browser/ui/browser_window.h"

namespace glic {

// Simple focused browser manager implementation for single browser strategies
// where the browser window can never go out of focus.
class GlicEmptyFocusedBrowserManager
    : public GlicFocusedBrowserManagerInterface {
 public:
  GlicEmptyFocusedBrowserManager();
  ~GlicEmptyFocusedBrowserManager() override;
  GlicEmptyFocusedBrowserManager(const GlicEmptyFocusedBrowserManager&) =
      delete;
  GlicEmptyFocusedBrowserManager& operator=(
      const GlicEmptyFocusedBrowserManager&) = delete;

  // GlicFocusedBrowserManagerInterface implementation.
  using FocusedBrowserChangedCallback =
      base::RepeatingCallback<void(BrowserWindowInterface* candidate,
                                   BrowserWindowInterface* focused)>;
  base::CallbackListSubscription AddFocusedBrowserChangedCallback(
      FocusedBrowserChangedCallback callback) override;
  base::CallbackListSubscription AddActiveBrowserChangedCallback(
      base::RepeatingCallback<void(BrowserWindowInterface*)> callback) override;
  BrowserWindowInterface* GetFocusedBrowser() const override;
  BrowserWindowInterface* GetActiveBrowser() const override;
  void OnGlicWindowActivationChanged(bool active) override {}
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_EMPTY_FOCUSED_BROWSER_MANAGER_H_
