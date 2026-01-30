// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UTILS_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UTILS_H_

namespace content {
class WebContents;
}  // namespace content

namespace contextual_tasks {

class ContextualTasksUIInterface;

// Finds the UI interface associated with the given WebContents. Returns nullptr
// if the `web_contents` does not have an associated UI.
ContextualTasksUIInterface* GetWebUiInterface(
    content::WebContents* web_contents);

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_UTILS_H_
