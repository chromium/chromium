// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_LOCATION_BAR_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_LOCATION_BAR_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/contextual_tasks/location_bar_stub.h"
#include "chrome/browser/ui/content_settings/content_setting_image_view_delegate.h"

class BrowserWindowInterface;
class ChipController;
class ContentSettingBubbleModelDelegate;
class PermissionDashboardController;

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

class ContextualTasksPermissionDashboard;
class ContextualTasksSidePanelCoordinator;

// Custom LocationBar implementation for the contextual tasks side panel
// toolbar. Inherits from LocationBarStub to avoid boilerplate for unused
// Omnibox methods.
class ContextualTasksLocationBar : public LocationBarStub,
                                   public ContentSettingImageViewDelegate {
 public:
  explicit ContextualTasksLocationBar(BrowserWindowInterface* browser_window);
  ContextualTasksLocationBar(const ContextualTasksLocationBar&) = delete;
  ContextualTasksLocationBar& operator=(const ContextualTasksLocationBar&) =
      delete;
  ~ContextualTasksLocationBar() override;

  ContextualTasksPermissionDashboard* permission_dashboard() {
    return permission_dashboard_.get();
  }
  const ContextualTasksPermissionDashboard* permission_dashboard() const {
    return permission_dashboard_.get();
  }

  // LocationBar overrides:
  // Returns the side panel's active content page, i.e. the page that
  // permissions apply to. This is swapped whenever the active task changes, so
  // it is resolved on every call rather than cached.
  content::WebContents* GetWebContents() override;
  BrowserWindowInterface* GetBrowser() override;
  bool IsEditingOrEmpty() const override;
  ui::TrackedElement* GetAnchorOrNull() override;
  void InvalidateLayout() override;
  bool IsDrawn() const override;
  bool IsFullscreen() const override;
  std::optional<bubble_anchor_util::AnchorConfiguration> GetChipAnchor()
      override;
  ChipController* GetChipController() override;
  PermissionDashboardController* GetPermissionDashboardController() override;
  void OnChanged() override;
  void UpdateContentSettingsIcons() override;

  // ContentSettingImageViewDelegate overrides:
  bool ShouldHideContentSettingImage() override;
  content::WebContents* GetContentSettingWebContents() override;
  ContentSettingBubbleModelDelegate* GetContentSettingBubbleModelDelegate()
      override;

  // Returns the side panel's WebUI toolbar page, which hosts the chip DOM
  // elements. This is a different page from `GetWebContents()` and is only used
  // for `ui::TrackedElement` lookups. Not a LocationBar override.
  content::WebContents* GetToolbarWebContents() const;

 private:
  // Resolves the coordinator that owns both the toolbar and the per-task
  // `WebContents`. Looked up on demand because the coordinator lives
  // the longest.
  ContextualTasksSidePanelCoordinator* GetCoordinator() const;

  raw_ptr<BrowserWindowInterface> browser_window_;
  std::unique_ptr<ContextualTasksPermissionDashboard> permission_dashboard_;
  std::unique_ptr<PermissionDashboardController>
      permission_dashboard_controller_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_LOCATION_BAR_H_
