// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_single_browser_focused_browser_manager.h"

namespace glic {

GlicSingleBrowserFocusedBrowserManager::GlicSingleBrowserFocusedBrowserManager(
    BrowserWindowInterface* bwi)
    : bwi_(bwi) {}

GlicSingleBrowserFocusedBrowserManager::
    ~GlicSingleBrowserFocusedBrowserManager() = default;

base::CallbackListSubscription
GlicSingleBrowserFocusedBrowserManager::AddFocusedBrowserChangedCallback(
    FocusedBrowserChangedCallback callback) {
  return {};
}

base::CallbackListSubscription
GlicSingleBrowserFocusedBrowserManager::AddActiveBrowserChangedCallback(
    base::RepeatingCallback<void(BrowserWindowInterface*)> callback) {
  return {};
}

BrowserWindowInterface*
GlicSingleBrowserFocusedBrowserManager::GetFocusedBrowser() const {
  return bwi_;
}

BrowserWindowInterface*
GlicSingleBrowserFocusedBrowserManager::GetActiveBrowser() const {
  // TODO(b:441552043): pull a default implementation for vanilla active browser
  // tracking.
  return bwi_;
}

}  // namespace glic
