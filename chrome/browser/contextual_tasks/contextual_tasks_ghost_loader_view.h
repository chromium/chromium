// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_GHOST_LOADER_VIEW_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_GHOST_LOADER_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/webview/webview.h"

namespace content {
class BrowserContext;
}

namespace contextual_tasks {

// A WebUI-based placeholder loading component for the Contextual Tasks side
// panel. It displays shimmering placeholder cards overlaid on top of the
// results web contents while results or queries are loading.
class ContextualTasksGhostLoaderView : public views::WebView {
  METADATA_HEADER(ContextualTasksGhostLoaderView, views::WebView)

 public:
  explicit ContextualTasksGhostLoaderView(
      content::BrowserContext* browser_context);
  ContextualTasksGhostLoaderView(const ContextualTasksGhostLoaderView&) =
      delete;
  ContextualTasksGhostLoaderView& operator=(
      const ContextualTasksGhostLoaderView&) = delete;
  ~ContextualTasksGhostLoaderView() override;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_GHOST_LOADER_VIEW_H_
