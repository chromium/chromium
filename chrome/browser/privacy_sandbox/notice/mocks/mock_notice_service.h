// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_MOCKS_MOCK_NOTICE_SERVICE_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_MOCKS_MOCK_NOTICE_SERVICE_H_

#include <vector>

#include "chrome/browser/privacy_sandbox/notice/mocks/mock_desktop_view_manager.h"
#include "chrome/browser/privacy_sandbox/notice/notice.mojom-forward.h"
#include "chrome/browser/privacy_sandbox/notice/notice_service_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {
class BrowserContext;
}

class KeyedService;

namespace privacy_sandbox {

enum class SurfaceType;

class MockPrivacySandboxNoticeService
    : public PrivacySandboxNoticeServiceInterface {
 public:
  MockPrivacySandboxNoticeService();

  MockPrivacySandboxNoticeService(const MockPrivacySandboxNoticeService&) =
      delete;
  MockPrivacySandboxNoticeService& operator=(
      const MockPrivacySandboxNoticeService&) = delete;

  ~MockPrivacySandboxNoticeService() override;

  MOCK_METHOD(std::vector<notice::mojom::PrivacySandboxNotice>,
              GetRequiredNotices,
              (SurfaceType),
              (override));

  MOCK_METHOD(void,
              EventOccurred,
              ((std::pair<notice::mojom::PrivacySandboxNotice, SurfaceType>),
               notice::mojom::PrivacySandboxNoticeEvent),
              (override));

#if !BUILDFLAG(IS_ANDROID)
  MOCK_METHOD(DesktopViewManagerInterface*,
              GetDesktopViewManager,
              (),
              (override));
#endif  // !BUILDFLAG(IS_ANDROID)
 private:
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<MockDesktopViewManager> mock_desktop_view_manager_;
#endif  // !BUILDFLAG(IS_ANDROID)
};

std::unique_ptr<KeyedService> BuildMockPrivacySandboxNoticeService(
    content::BrowserContext* context);

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_NOTICE_MOCKS_MOCK_NOTICE_SERVICE_H_
