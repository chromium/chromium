// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_single_browser_focused_tab_manager.h"

namespace glic {

GlicSingleBrowserFocusedTabManager::GlicSingleBrowserFocusedTabManager(
    BrowserWindowInterface* browser_interface)
    : browser_interface_(browser_interface) {
  if (browser_interface) {
    active_tab_subscription_ =
        browser_interface->RegisterActiveTabDidChange(base::BindRepeating(
            &GlicSingleBrowserFocusedTabManager::OnActiveTabChanged,
            base::Unretained(this)));
  }
}

GlicSingleBrowserFocusedTabManager::~GlicSingleBrowserFocusedTabManager() =
    default;

base::CallbackListSubscription
GlicSingleBrowserFocusedTabManager::AddFocusedTabChangedCallback(
    FocusedTabChangedCallback callback) {
  return focused_callback_list_.Add(std::move(callback));
}

void GlicSingleBrowserFocusedTabManager::OnActiveTabChanged(
    BrowserWindowInterface* browser_interface) {
  // TODO(crbug.com/441552043): Handle web contents observing.
  NotifyFocusedTabChanged();
}

FocusedTabData GlicSingleBrowserFocusedTabManager::GetFocusedTabData() {
  tabs::TabInterface* focused_tab =
      browser_interface_ ? browser_interface_->GetActiveTabInterface()
                         : nullptr;
  // TODO(crbug.com/441552043): Handle tab validity checks.
  return focused_tab
             ? FocusedTabData(focused_tab)
             : FocusedTabData(std::string("focused tab disappeared"), nullptr);
}

void GlicSingleBrowserFocusedTabManager::NotifyFocusedTabChanged() {
  focused_callback_list_.Notify(GetFocusedTabData());
}

}  // namespace glic
