// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_FOCUSED_BROWSER_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_FOCUSED_BROWSER_MANAGER_INTERFACE_H_

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "chrome/browser/ui/browser_window.h"  // nogncheck

namespace glic {

// Responsible for managing which browser window is considered "focused".
class GlicFocusedBrowserManagerInterface {
 public:
  GlicFocusedBrowserManagerInterface() = default;
  virtual ~GlicFocusedBrowserManagerInterface() = default;
  GlicFocusedBrowserManagerInterface(
      const GlicFocusedBrowserManagerInterface&) = delete;
  GlicFocusedBrowserManagerInterface& operator=(
      const GlicFocusedBrowserManagerInterface&) = delete;

  // Callback for changes to the focused browser window, or the candidate
  // to be focused.
  using FocusedBrowserChangedCallback =
      base::RepeatingCallback<void(BrowserWindowInterface* candidate,
                                   BrowserWindowInterface* focused)>;
  virtual base::CallbackListSubscription AddFocusedBrowserChangedCallback(
      FocusedBrowserChangedCallback callback) = 0;

  // Callback for changes to the active browser window. This provides the value
  // of GetActiveBrowser().
  virtual base::CallbackListSubscription AddActiveBrowserChangedCallback(
      base::RepeatingCallback<void(BrowserWindowInterface*)> callback) = 0;

  // Returns the currently focused browser window, if there is one.
  virtual BrowserWindowInterface* GetFocusedBrowser() const = 0;

  // Returns either the currently focused browser window, or the most recently
  // focused window if the Glic panel is focused instead. This can return a
  // browser that belongs to a different profile.
  virtual BrowserWindowInterface* GetActiveBrowser() const = 0;

  // Callback for when a detached instance's window activation changes.
  virtual void OnGlicWindowActivationChanged(bool active) = 0;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_FOCUSED_BROWSER_MANAGER_INTERFACE_H_
