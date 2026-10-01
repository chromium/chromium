// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_LOCATION_BAR_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_LOCATION_BAR_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/contextual_tasks/location_bar_stub.h"
#include "chrome/browser/ui/content_settings/content_setting_image_view_delegate.h"
#include "chrome/browser/ui/views/location_bar/webui_content_setting_image_control.h"

class BrowserWindowInterface;
class ChipController;
class ContentSettingBubbleModelDelegate;
class PermissionDashboardController;
class WebUIPermissionDashboard;

namespace bubble_anchor_util {
struct AnchorConfiguration;
}

namespace content {
class WebContents;
}

namespace ui {
class TrackedElement;
}

namespace contextual_tasks {

class ContextualTasksSidePanelCoordinator;

// Custom LocationBar implementation for the contextual tasks side panel
// toolbar. Inherits from LocationBarStub to avoid boilerplate for unused
// Omnibox methods.
class ContextualTasksLocationBar : public LocationBarStub,
                                   public ContentSettingImageViewDelegate {
 public:
  // `state_changed_callback` is run whenever a change occurs that the
  // permission controller + toolbar WebUI needs to be told about (chip
  // mutations, content setting updates). It is owned by
  // `ContextualTasksPermissionController`, which outlives this.
  ContextualTasksLocationBar(BrowserWindowInterface* browser_window,
                             base::RepeatingClosure state_changed_callback);
  ContextualTasksLocationBar(const ContextualTasksLocationBar&) = delete;
  ContextualTasksLocationBar& operator=(const ContextualTasksLocationBar&) =
      delete;
  ~ContextualTasksLocationBar() override;

  WebUIPermissionDashboard* permission_dashboard() {
    return permission_dashboard_.get();
  }
  const WebUIPermissionDashboard* permission_dashboard() const {
    return permission_dashboard_.get();
  }

  // LocationBar overrides:
  // Returns the `WebContents` hosting the currently active task's guest web
  // content in the side panel (i.e. the page that permissions apply to).
  // Updated via `Update()` when the active task changes.
  content::WebContents* GetWebContents() override;
  BrowserWindowInterface* GetBrowser() override;
  bool IsEditingOrEmpty() const override;
  ui::TrackedElement* GetAnchorOrNull() override;
  ui::ElementContext GetElementContext() const override;
  void InvalidateLayout() override;
  bool IsDrawn() const override;
  bool IsFullscreen() const override;
  std::optional<bubble_anchor_util::AnchorConfiguration> GetChipAnchor()
      override;
  ChipController* GetChipController() override;
  PermissionDashboardController* GetPermissionDashboardController() override;
  void OnChanged() override;
  void UpdateContentSettingsIcons() override;
  void Update(content::WebContents* contents) override;

  // ContentSettingImageViewDelegate overrides:
  bool ShouldHideContentSettingImage() override;
  content::WebContents* GetContentSettingWebContents() override;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override;
  // Returns the `WebContents` of the side panel's WebUI toolbar.
  // Unlike `GetWebContents()`, this is shared across all tasks
  // per window and is used for `ui::TrackedElement` anchor lookups.
  // Not a LocationBar override.
  content::WebContents* GetToolbarWebContents() const;

 private:
  // Resolves the coordinator that owns both the toolbar and the per-task
  // `WebContents`. Looked up on demand because the coordinator lives
  // the longest.
  ContextualTasksSidePanelCoordinator* GetCoordinator() const;

  raw_ptr<BrowserWindowInterface> browser_window_;
  base::RepeatingClosure state_changed_callback_;

  // The web contents of the currently active task in the side panel, updated
  // via `Update()` when switching tasks.
  base::WeakPtr<content::WebContents> web_contents_;

  // Owns the `ContentSettingImageModel`s that drive the indicator chip.
  // Declared before `permission_dashboard_controller_` so that it is destroyed
  // after it: `PermissionDashboardController` retains a raw pointer to the
  // model last passed to its `Update()`.
  WebUIContentSettingImageControl content_setting_image_control_{this};

  std::unique_ptr<WebUIPermissionDashboard> permission_dashboard_;
  std::unique_ptr<PermissionDashboardController>
      permission_dashboard_controller_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_LOCATION_BAR_H_
