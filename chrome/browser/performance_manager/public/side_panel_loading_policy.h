// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_SIDE_PANEL_LOADING_POLICY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_SIDE_PANEL_LOADING_POLICY_H_

namespace content {
class WebContents;
}

namespace performance_manager::execution_context_priority {

// Marks `web_contents` as the main contents of a Side Panel, which will ensure
// that it loads at high priority, even if it is not visible.
void MarkAsSidePanel(content::WebContents* web_contents);

}  // namespace performance_manager::execution_context_priority

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PUBLIC_SIDE_PANEL_LOADING_POLICY_H_
