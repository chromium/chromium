// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_HOST_DESKTOP_IMPL_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_HOST_DESKTOP_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_observer.h"
#include "content/public/browser/web_contents.h"

class SidePanelEntryScope;
class SidePanelRegistry;
class SidePanelUI;

namespace views {
class View;
}  // namespace views

namespace contextual_tasks {

class ContextualTasksWebView;

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
  void AddObserver(ContextualTasksPanelHost::Observer* observer) override;
  void RemoveObserver(ContextualTasksPanelHost::Observer* observer) override;
  void Show(ContextualTasksPanelHost::AnimationStyle animation) override;
  void Close(ContextualTasksPanelHost::AnimationStyle animation) override;
  bool IsPanelInitialized() override;
  bool IsPanelOpenForContextualTask() const override;
  // crbug.com/477278769: Do not open side panel if glic side panel is already
  // open on tab changed.
  bool IsPanelSuppressed() const override;
  void SetPanelSuppressedForTesting(bool suppressed) override;
  content::WebContents* GetWebContents() override;
  void SetWebContents(content::WebContents* web_contents) override;

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

  // For testing only. Create the side panel web view.
  std::unique_ptr<views::View> CreateSidePanelView(SidePanelEntryScope& scope);

 private:
  // Notify observers about surface state changes.
  void NotifySurfaceStateChanged(
      ContextualTasksPanelHost::SurfaceState state,
      ContextualTasksPanelHost::StateChangeReason reason);

  // Create a `SidePanelEntry` for ContextualTasks and register it with the
  // global `SidePanelRegistry`.
  void CreateAndRegisterEntry();

  // Shows side panel, transitioning from active tab content's bounds.
  void ShowFromTab();

  // Browser window of the current panel.
  const raw_ptr<BrowserWindowInterface> browser_window_;

  // Used to show, close, and query the state of the side panel.
  const raw_ptr<SidePanelUI> side_panel_ui_;

  // WebView of the current panel.
  base::WeakPtr<ContextualTasksWebView> web_view_ = nullptr;

  // Observers to inform when the panel state changes.
  base::ObserverList<ContextualTasksPanelHost::Observer> observers_;

  // True if the panel should be suppressed for testing.
  bool suppressed_for_testing_ = false;

  base::WeakPtrFactory<ContextualTasksPanelHostDesktopImpl> weak_factory_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_HOST_DESKTOP_IMPL_H_
