// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_single_browser_focused_tab_manager.h"

#include "chrome/browser/glic/host/context/glic_sharing_utils.h"

namespace glic {

GlicSingleBrowserFocusedTabManager::GlicSingleBrowserFocusedTabManager(
    BrowserWindowInterface* browser_interface)
    : browser_interface_(browser_interface) {
  if (browser_interface) {
    active_tab_subscription_ =
        browser_interface->RegisterActiveTabDidChange(base::BindRepeating(
            &GlicSingleBrowserFocusedTabManager::OnActiveTabChanged,
            base::Unretained(this)));
    focused_tab_data_observer_ = std::make_unique<TabDataObserver>(
        GetFocusedTabData().focus()->GetContents(),
        base::BindRepeating(
            &GlicSingleBrowserFocusedTabManager::FocusedTabDataChanged,
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

base::CallbackListSubscription
GlicSingleBrowserFocusedTabManager::AddFocusedTabDataChangedCallback(
    FocusedTabDataChangedCallback callback) {
  return focused_data_callback_list_.Add(std::move(callback));
}

bool GlicSingleBrowserFocusedTabManager::IsTabFocused(
    tabs::TabHandle tab_handle) const {
  auto* tab = tab_handle.Get();
  if (!tab) {
    return false;
  }
  tabs::TabInterface* focused_tab = GetFocusedTabData().focus();
  if (!focused_tab) {
    return false;
  }
  return tab == focused_tab;
}

void GlicSingleBrowserFocusedTabManager::FocusedTabDataChanged(
    TabDataChange change) {
  NotifyFocusedTabDataChanged(std::move(change));
}

void GlicSingleBrowserFocusedTabManager::OnActiveTabChanged(
    BrowserWindowInterface* browser_interface) {
  FocusedTabData focused_tab_data = GetFocusedTabData();

  // Override any existing observer (even if focus is not null)
  focused_tab_data_observer_ = std::make_unique<TabDataObserver>(
      focused_tab_data.focus()->GetContents(),
      base::BindRepeating(
          &GlicSingleBrowserFocusedTabManager::FocusedTabDataChanged,
          base::Unretained(this)));

  NotifyFocusedTabChanged(focused_tab_data);
}

FocusedTabData GlicSingleBrowserFocusedTabManager::GetFocusedTabData() const {
  tabs::TabInterface* focused_tab =
      browser_interface_ ? browser_interface_->GetActiveTabInterface()
                         : nullptr;
  if (!focused_tab) {
    return FocusedTabData(std::string("focused tab disappeared"), nullptr);
  }

  if (!IsTabValidForSharing(focused_tab->GetContents())) {
    return FocusedTabData(std::string("no focusable tab"), focused_tab);
  }

  return FocusedTabData(focused_tab);
}

FocusedTabData GlicSingleBrowserFocusedTabManager::GetFocusedTabData() {
  return const_cast<const GlicSingleBrowserFocusedTabManager*>(this)
      ->GetFocusedTabData();
}

void GlicSingleBrowserFocusedTabManager::NotifyFocusedTabChanged(
    const FocusedTabData& focused_tab_data) {
  focused_callback_list_.Notify(focused_tab_data);
}

void GlicSingleBrowserFocusedTabManager::NotifyFocusedTabDataChanged(
    TabDataChange change) {
  focused_data_callback_list_.Notify(change.tab_data.get());
}

}  // namespace glic
