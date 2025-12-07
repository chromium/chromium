// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_empty_focused_browser_manager.h"

namespace glic {

GlicEmptyFocusedBrowserManager::GlicEmptyFocusedBrowserManager() = default;

GlicEmptyFocusedBrowserManager::~GlicEmptyFocusedBrowserManager() = default;

base::CallbackListSubscription
GlicEmptyFocusedBrowserManager::AddFocusedBrowserChangedCallback(
    FocusedBrowserChangedCallback callback) {
  return {};
}

base::CallbackListSubscription
GlicEmptyFocusedBrowserManager::AddActiveBrowserChangedCallback(
    base::RepeatingCallback<void(BrowserWindowInterface*)> callback) {
  return {};
}

BrowserWindowInterface* GlicEmptyFocusedBrowserManager::GetFocusedBrowser()
    const {
  return nullptr;
}

BrowserWindowInterface* GlicEmptyFocusedBrowserManager::GetActiveBrowser()
    const {
  // TODO(b:441552043): pull a default implementation for vanilla active browser
  // tracking.
  return nullptr;
}

}  // namespace glic
