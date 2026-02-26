// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UTILS_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UTILS_H_

#include "mojo/public/cpp/bindings/remote.h"

namespace content {
class WebContents;
}  // namespace content

namespace contextual_search {
enum class ContextualSearchSource;
}  // namespace contextual_search

namespace contextual_tasks {
namespace mojom {
class Page;
}  // namespace mojom

class ContextualTasksUIInterface;

// Finds the UI interface associated with the given WebContents. Returns nullptr
// if the `web_contents` does not have an associated UI.
ContextualTasksUIInterface* GetWebUiInterface(
    content::WebContents* web_contents);

// Shows the error page on the given page and records the error page shown
// metric for the given source.
void ShowAndRecordErrorPage(mojo::Remote<contextual_tasks::mojom::Page>& page,
                            contextual_search::ContextualSearchSource source);

// Records the error page shown metric for the given source.
void RecordErrorPageShown(contextual_search::ContextualSearchSource source);

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UTILS_H_
