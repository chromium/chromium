// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_HOST_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_HOST_H_

#include <memory>

class BrowserWindowInterface;

namespace content {
class WebContents;
}  // namespace content

namespace contextual_tasks {

// Interface for supporting view creation and UI interaction logic for the
// ContextualTasksPanelController. It is responsible for abstracting away
// platform-specific differences (e.g. bottom sheet on mobile vs. side panel on
// desktop).
class ContextualTasksPanelHost {
 public:
  static std::unique_ptr<ContextualTasksPanelHost> Create(
      BrowserWindowInterface* browser_window);

  virtual ~ContextualTasksPanelHost() = default;

  // Visibility commands.
  // Show the panel. If |transition_from_tab| is true, trigger the panel content
  // to animate from the active tab content's bounds.
  virtual void Show(bool transition_from_tab = false) = 0;
  // Close the panel.
  virtual void Close() = 0;

  // Content management.
  // Configures the panel to display the provided WebContents.
  virtual void SetWebContents(content::WebContents* web_contents) = 0;

  // Promotion.
  // Transitions the panel content into a full browser tab.
  virtual void PromoteToTab() = 0;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PANEL_HOST_H_
