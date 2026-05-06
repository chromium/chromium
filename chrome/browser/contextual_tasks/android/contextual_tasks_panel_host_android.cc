// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/android/contextual_tasks_panel_host_android.h"

#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/android/window_android.h"

namespace contextual_tasks {

// static
std::unique_ptr<ContextualTasksPanelHost> ContextualTasksPanelHost::Create(
    BrowserWindowInterface* browser_window) {
  return std::make_unique<ContextualTasksPanelHostAndroid>(browser_window);
}

ContextualTasksPanelHostAndroid::ContextualTasksPanelHostAndroid(
    BrowserWindowInterface* browser_window)
    : browser_window_(browser_window) {}

ContextualTasksPanelHostAndroid::~ContextualTasksPanelHostAndroid() = default;

void ContextualTasksPanelHostAndroid::AddObserver(
    ContextualTasksPanelHost::Observer* observer) {
  observers_.AddObserver(observer);
}

void ContextualTasksPanelHostAndroid::RemoveObserver(
    ContextualTasksPanelHost::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ContextualTasksPanelHostAndroid::Show(AnimationStyle animation_style) {
  if (auto* bridge = GetOrCreateBridge()) {
    if (bridge->Show(/*animate=*/false, /*starts_expanded=*/true)) {
      is_open_ = true;
      observers_.Notify(
          &ContextualTasksPanelHost::Observer::OnSurfaceStateChanged,
          SurfaceState::kVisible, StateChangeReason::kSystemAction);
    }
  }
}

void ContextualTasksPanelHostAndroid::Close(AnimationStyle animation_style) {
  if (!is_open_) {
    return;
  }
  if (auto* bridge = GetOrCreateBridge()) {
    bridge->Close(/* animate= */ true);
  }
}

bool ContextualTasksPanelHostAndroid::IsPanelInitialized() {
  return true;
}

bool ContextualTasksPanelHostAndroid::IsPanelOpenForContextualTask() const {
  return is_open_;
}

bool ContextualTasksPanelHostAndroid::IsPanelSuppressed() const {
  return suppressed_for_testing_;
}

void ContextualTasksPanelHostAndroid::SetPanelSuppressedForTesting(
    bool suppressed) {
  suppressed_for_testing_ = suppressed;
}

content::WebContents* ContextualTasksPanelHostAndroid::GetWebContents() {
  return web_contents_.get();
}

void ContextualTasksPanelHostAndroid::SetWebContents(
    content::WebContents* web_contents) {
  if (web_contents_ == web_contents) {
    return;
  }

  web_contents_ = web_contents;

  if (!web_contents_) {
    return;
  }

  web_contents_->SetDelegate(this);
  if (auto* bridge = GetOrCreateBridge()) {
    bridge->SetWebContents(web_contents);
    if (is_open_) {
      bridge->Show(/*animate=*/false, /*starts_expanded=*/true);
    }
  }
}

void ContextualTasksPanelHostAndroid::OnClosed() {
  is_open_ = false;
  observers_.Notify(&ContextualTasksPanelHost::Observer::OnSurfaceStateChanged,
                    SurfaceState::kClosed, StateChangeReason::kUserAction);
}

void ContextualTasksPanelHostAndroid::OnSuppressed() {}

void ContextualTasksPanelHostAndroid::OnOpened(bool is_expanded) {}

content::WebContents* ContextualTasksPanelHostAndroid::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params,
    base::OnceCallback<void(content::NavigationHandle&)>
        navigation_handle_callback) {
  if (browser_window_) {
    return browser_window_->OpenURL(params,
                                    std::move(navigation_handle_callback));
  }
  return nullptr;
}

context_sharing::TabBottomSheetBridge*
ContextualTasksPanelHostAndroid::GetOrCreateBridge() {
  if (!bridge_) {
    TabAndroid* tab_android = GetTabAndroid();
    if (!tab_android) {
      return nullptr;
    }
    bridge_ = std::make_unique<context_sharing::TabBottomSheetBridge>(
        this, tab_android,
        context_sharing::TabBottomSheetClientType::kContextualTasks);
  }
  return bridge_.get();
}

TabAndroid* ContextualTasksPanelHostAndroid::GetTabAndroid() const {
  TabListInterface* tab_list = TabListInterface::From(browser_window_);
  if (!tab_list) {
    return nullptr;
  }
  tabs::TabInterface* active_tab = tab_list->GetActiveTab();
  if (!active_tab) {
    return nullptr;
  }
  return TabAndroid::FromTabHandle(active_tab->GetHandle());
}

}  // namespace contextual_tasks
