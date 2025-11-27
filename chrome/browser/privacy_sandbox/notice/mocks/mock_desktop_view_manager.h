// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_MOCKS_MOCK_DESKTOP_VIEW_MANAGER_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_MOCKS_MOCK_DESKTOP_VIEW_MANAGER_H_

#include <optional>

#include "chrome/browser/privacy_sandbox/notice/desktop_view_manager.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom-forward.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace privacy_sandbox {

class MockDesktopViewManager : public DesktopViewManagerInterface {
 public:
  MockDesktopViewManager();
  ~MockDesktopViewManager() override;

  MOCK_METHOD(void,
              HandleChromeOwnedPageNavigation,
              (BrowserWindowInterface* browser_interface),
              (override));
  MOCK_METHOD(void,
              OnEventOccurred,
              (notice::mojom::PrivacySandboxNotice notice,
               notice::mojom::PrivacySandboxNoticeEvent event),
              (override));
  MOCK_METHOD(void, AddObserver, (Observer*), (override));
  MOCK_METHOD(void, RemoveObserver, (Observer*), (override));
};

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_MOCKS_MOCK_DESKTOP_VIEW_MANAGER_H_
