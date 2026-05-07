// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_PANEL_HOST_DESKTOP_ANDROID_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_PANEL_HOST_DESKTOP_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/co_browse_views_bridge.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host.h"
#include "chrome/browser/ui/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/side_panel/side_panel_native_view.h"
#include "content/public/browser/web_contents_delegate.h"

class BrowserWindowInterface;
class SidePanelEntry;
class SidePanelEntryScope;
class SidePanelUI;

namespace content {
class WebContents;
}

namespace contextual_tasks {

// Host class for the Contextual Tasks side panel on Android Desktop.
// This class manages the lifecycle of the side panel entry, creates the
// CoBrowse view, and handles communication between the side panel and the
// browser window.
class ContextualTasksPanelHostDesktopAndroid
    : public ContextualTasksPanelHost,
      public SidePanelEntryObserver,
      public content::WebContentsDelegate {
 public:
  explicit ContextualTasksPanelHostDesktopAndroid(
      BrowserWindowInterface* browser_window);

  ~ContextualTasksPanelHostDesktopAndroid() override;

  // ContextualTasksPanelHost implementation:
  void AddObserver(ContextualTasksPanelHost::Observer* observer) override;
  void RemoveObserver(ContextualTasksPanelHost::Observer* observer) override;
  void Show(AnimationStyle animation) override;
  void Close(AnimationStyle animation) override;
  bool IsPanelInitialized() override;
  bool IsPanelOpenForContextualTask() const override;
  bool IsPanelSuppressed() const override;
  void SetPanelSuppressedForTesting(bool suppressed) override;
  content::WebContents* GetWebContents() override;
  void SetWebContents(content::WebContents* web_contents) override;

  // SidePanelEntryObserver implementation:
  void OnEntryHiddenWithReason(SidePanelEntry* entry,
                               SidePanelEntryHideReason reason) override;
  void OnEntryShown(SidePanelEntry* entry) override;

  // content::WebContentsDelegate implementation:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override;

 private:
  void MaybeRegisterEntry();
  SidePanelUI* GetSidePanelUI() const;
  SidePanelNativeView CreateView(SidePanelEntryScope& scope);

  // Ensures that the CoBrowseViewsBridge is created lazily.
  //
  // It will attempt to get the active tab from the browser window to initialize
  // the bridge. Returns true if the bridge exists.
  bool MaybeCreateBridge();

  void NotifySurfaceStateChanged(
      ContextualTasksPanelHost::SurfaceState state,
      ContextualTasksPanelHost::StateChangeReason reason);

  const raw_ptr<BrowserWindowInterface> browser_window_;
  base::ObserverList<ContextualTasksPanelHost::Observer> observers_;

  // We use ScopedObservation to safely detach as an observer when the host
  // is torn down.
  base::ScopedObservation<SidePanelEntry, SidePanelEntryObserver>
      side_panel_entry_observation_{this};

  raw_ptr<content::WebContents> web_contents_ = nullptr;
  base::WeakPtr<tabs::TabInterface> tab_ref_;
  std::unique_ptr<context_sharing::CoBrowseViewsBridge> co_browse_views_bridge_;

  bool suppressed_for_testing_ = false;
  bool is_open_ = false;

  base::WeakPtrFactory<ContextualTasksPanelHostDesktopAndroid> weak_factory_{
      this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_PANEL_HOST_DESKTOP_ANDROID_H_
