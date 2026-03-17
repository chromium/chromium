// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/android/contextual_tasks_panel_host_android.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "content/public/browser/web_contents.h"

// TODO(shobiz): Implement the methods in this file. See crrev.com/c/7630475.
namespace contextual_tasks {

// static
std::unique_ptr<ContextualTasksPanelHost> ContextualTasksPanelHost::Create(
    BrowserWindowInterface* browser_window) {
  return std::make_unique<ContextualTasksPanelHostAndroid>(browser_window);
}

ContextualTasksPanelHostAndroid::ContextualTasksPanelHostAndroid(
    BrowserWindowInterface* browser_window) {}

ContextualTasksPanelHostAndroid::~ContextualTasksPanelHostAndroid() = default;

void ContextualTasksPanelHostAndroid::AddObserver(Observer* observer) {}

void ContextualTasksPanelHostAndroid::RemoveObserver(Observer* observer) {}

void ContextualTasksPanelHostAndroid::Show(AnimationStyle animation) {}

void ContextualTasksPanelHostAndroid::Close(AnimationStyle animation) {}

bool ContextualTasksPanelHostAndroid::IsPanelInitialized() {
  return false;
}

bool ContextualTasksPanelHostAndroid::IsPanelOpenForContextualTask() const {
  return false;
}

bool ContextualTasksPanelHostAndroid::IsPanelSuppressed() const {
  return false;
}

void ContextualTasksPanelHostAndroid::SetPanelSuppressedForTesting(
    bool suppressed) {}

content::WebContents* ContextualTasksPanelHostAndroid::GetWebContents() {
  return nullptr;
}

void ContextualTasksPanelHostAndroid::SetWebContents(
    content::WebContents* web_contents) {}

}  // namespace contextual_tasks
