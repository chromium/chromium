// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_HOST_DESKTOP_IMPL_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_HOST_DESKTOP_IMPL_H_

#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_observer.h"

class BrowserWindowInterface;
class SidePanelUI;

namespace content {
class WebContents;
}  // namespace content

namespace contextual_tasks {

// Desktop-specific implementation of the `ContextualTasksPanelHost` interface.
// This class manages the display of the contextual tasks UI within the
// browser's side panel framework and coordinates with `SidePanelUI` and
// `SidePanelRegistry` to handle visibility and lifecycle events.
class ContextualTasksPanelHostDesktopImpl : public ContextualTasksPanelHost,
                                            public SidePanelEntryObserver {
 public:
  // For testing only. Use static ContextualTasksPanelHost::Create to get an
  // instance rather than explicitly constructing.
  ContextualTasksPanelHostDesktopImpl(BrowserWindowInterface* browser_window,
                                      SidePanelUI* side_panel_ui);

  // Disallow copy/assign.
  ContextualTasksPanelHostDesktopImpl(
      const ContextualTasksPanelHostDesktopImpl&) = delete;
  ContextualTasksPanelHostDesktopImpl& operator=(
      const ContextualTasksPanelHostDesktopImpl&) = delete;
  ~ContextualTasksPanelHostDesktopImpl() override;

  // ContextualTasksPanelHost:
  void Show(bool transition_from_tab) override;
  void Close() override;
  void SetWebContents(content::WebContents* web_contents) override;
  void PromoteToTab() override;

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

 private:
  // Browser window of the current panel.
  const raw_ptr<BrowserWindowInterface> browser_window_;

  // Used to show, close, and query the state of the side panel.
  const raw_ptr<SidePanelUI> side_panel_ui_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_HOST_DESKTOP_IMPL_H_
