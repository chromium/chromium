// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SCRIM_VIEW_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SCRIM_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/webview.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

namespace contextual_tasks {

// A WebUI-based scrim component for the Contextual Tasks side panel.
// It overlays the results web contents, hosting either shimmering
// placeholder cards while results are loading or the CSE-protected
// page error screen when page context is ineligible.
class ContextualTasksScrimView : public views::WebView {
  METADATA_HEADER(ContextualTasksScrimView, views::WebView)

 public:
  // Constructs the scrim view. `browser_context` must be non-null and is
  // retained by the underlying `views::WebView`.
  explicit ContextualTasksScrimView(content::BrowserContext* browser_context);
  ContextualTasksScrimView(const ContextualTasksScrimView&) = delete;
  ContextualTasksScrimView& operator=(const ContextualTasksScrimView&) = delete;
  ~ContextualTasksScrimView() override;

  // views::WebView:
  void SetWebContents(content::WebContents* web_contents) override;

  // Toggles whether the scrim displays the protected page error UI instead of
  // the shimmering ghost loader cards.
  void SetShowErrorPage(bool show_error_page);

  bool is_showing_error_page() const { return is_showing_error_page_; }

 private:
  bool is_showing_error_page_ = false;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_SCRIM_VIEW_H_
