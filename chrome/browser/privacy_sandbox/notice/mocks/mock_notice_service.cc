// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/mocks/mock_notice_service.h"

#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace privacy_sandbox {

using ::testing::NiceMock;
using ::testing::Return;

MockPrivacySandboxNoticeService::MockPrivacySandboxNoticeService() {
#if !BUILDFLAG(IS_ANDROID)
  mock_desktop_view_manager_ = std::make_unique<MockDesktopViewManager>();
  ON_CALL(*this, GetDesktopViewManager())
      .WillByDefault(Return(mock_desktop_view_manager_.get()));
#endif  // !BUILDFLAG(IS_ANDROID)
}
MockPrivacySandboxNoticeService::~MockPrivacySandboxNoticeService() = default;

std::unique_ptr<KeyedService> BuildMockPrivacySandboxNoticeService(
    content::BrowserContext* context) {
  return std::make_unique<NiceMock<MockPrivacySandboxNoticeService>>();
}

}  // namespace privacy_sandbox
