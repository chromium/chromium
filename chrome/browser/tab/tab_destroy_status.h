// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_TAB_DESTROY_STATUS_H_
#define CHROME_BROWSER_TAB_TAB_DESTROY_STATUS_H_

namespace tabs {

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome.browser.tab
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: TabDestroyStatus
enum class TabDestroyStatus {
  // No shutdown occurred (e.g. no WebContents or fast shutdown failed).
  NO_SHUTDOWN = 0,
  // Fast shutdown succeeded, terminating the renderer process immediately.
  FAST_SHUTDOWN = 1,
  // Slow shutdown initiated, deferring WebContents destruction for graceful
  // teardown.
  SLOW_SHUTDOWN = 2,
};

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_TAB_DESTROY_STATUS_H_
