// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_VIEW_MANAGER_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_VIEW_MANAGER_H_

namespace privacy_sandbox {

// This class will:
// 1. Manage the showing, hiding and closing of notices in the correct order on
// the desktop side.
// 2. Advance multi-step notices
// 3. Manage sticky behavior of notices across tabs
class DesktopViewManager {
 public:
  DesktopViewManager();
  virtual ~DesktopViewManager();
};

}  // namespace privacy_sandbox
#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_DESKTOP_VIEW_MANAGER_H_
