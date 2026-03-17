// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_PANEL_HOST_ANDROID_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_PANEL_HOST_ANDROID_H_

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host.h"

class BrowserWindowInterface;

namespace content {
class WebContents;
}

namespace contextual_tasks {

// Android implementation of ContextualTasksPanelHost using a bottom sheet.
class ContextualTasksPanelHostAndroid : public ContextualTasksPanelHost {
 public:
  explicit ContextualTasksPanelHostAndroid(
      BrowserWindowInterface* browser_window);
  ~ContextualTasksPanelHostAndroid() override;

  // ContextualTasksPanelHost:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void Show(AnimationStyle animation) override;
  void Close(AnimationStyle animation) override;
  bool IsPanelInitialized() override;
  bool IsPanelOpenForContextualTask() const override;
  bool IsPanelSuppressed() const override;
  void SetPanelSuppressedForTesting(bool suppressed) override;
  content::WebContents* GetWebContents() override;
  void SetWebContents(content::WebContents* web_contents) override;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_ANDROID_CONTEXTUAL_TASKS_PANEL_HOST_ANDROID_H_
