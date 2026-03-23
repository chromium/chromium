// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_PANEL_HOST_ANDROID_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_PANEL_HOST_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/context_sharing/tab_bottom_sheet/android/tab_bottom_sheet_bridge.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host.h"

class BrowserWindowInterface;
class TabAndroid;

namespace content {
class WebContents;
}

namespace contextual_tasks {

// Android implementation of ContextualTasksPanelHost using a bottom sheet.
class ContextualTasksPanelHostAndroid
    : public ContextualTasksPanelHost,
      public context_sharing::TabBottomSheetBridge::Observer {
 public:
  explicit ContextualTasksPanelHostAndroid(
      BrowserWindowInterface* browser_window);
  ~ContextualTasksPanelHostAndroid() override;

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

  // context_sharing::TabBottomSheetBridge::Observer:
  void OnClose() override;

 private:
  // Helper method to get the bridge, creating it if necessary. This is because
  // `this` is not a tab-scoped object, so it might get instantiated before any
  // tabs are created. If the tab is not available, it will return nullptr.
  context_sharing::TabBottomSheetBridge* GetOrCreateBridge();

  // Retrieves the TabAndroid associated with the active tab in the browser
  // window. Returns nullptr if no active tab is found or if it doesn't have an
  // associated TabAndroid.
  TabAndroid* GetTabAndroid() const;

  // The browser window this host is attached to. Must outlive this object.
  const raw_ptr<BrowserWindowInterface> browser_window_;
  base::ObserverList<ContextualTasksPanelHost::Observer> observers_;

  // Bridge to manage the native lifecycle and JNI interactions for the Java
  // bottom sheet. Access through GetOrCreateBridge() helper method.
  std::unique_ptr<context_sharing::TabBottomSheetBridge> bridge_;

  // The WebContents currently being displayed in the panel.
  raw_ptr<content::WebContents> web_contents_ = nullptr;

  // Tracks whether the panel is currently visible to the user.
  bool is_open_ = false;

  // Allows tests to suppress the actual showing of the panel.
  bool suppressed_for_testing_ = false;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_PANEL_HOST_ANDROID_H_
