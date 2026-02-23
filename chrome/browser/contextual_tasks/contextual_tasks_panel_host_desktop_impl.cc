// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host_desktop_impl.h"

#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace contextual_tasks {

// static
std::unique_ptr<ContextualTasksPanelHost> ContextualTasksPanelHost::Create(
    BrowserWindowInterface* browser_window) {
  return std::make_unique<ContextualTasksPanelHostDesktopImpl>(
      browser_window, browser_window->GetFeatures().side_panel_ui());
}

ContextualTasksPanelHostDesktopImpl::ContextualTasksPanelHostDesktopImpl(
    BrowserWindowInterface* browser_window,
    SidePanelUI* side_panel_ui)
    : browser_window_(browser_window), side_panel_ui_(side_panel_ui) {}

ContextualTasksPanelHostDesktopImpl::~ContextualTasksPanelHostDesktopImpl() =
    default;

void ContextualTasksPanelHostDesktopImpl::Show(bool transition_from_tab) {
  // TODO(crbug.com/478282903): Implement.
}

void ContextualTasksPanelHostDesktopImpl::Close() {
  // TODO(crbug.com/478282903): Implement.
}

void ContextualTasksPanelHostDesktopImpl::SetWebContents(
    content::WebContents* web_contents) {
  // TODO(crbug.com/478282903): Implement.
}

void ContextualTasksPanelHostDesktopImpl::PromoteToTab() {
  // TODO(crbug.com/478282903): Implement.
}

void ContextualTasksPanelHostDesktopImpl::OnEntryShown(SidePanelEntry* entry) {
  // TODO(crbug.com/478282903): Implement.
}

void ContextualTasksPanelHostDesktopImpl::OnEntryHidden(SidePanelEntry* entry) {
  // TODO(crbug.com/478282903): Implement.
}

}  // namespace contextual_tasks
