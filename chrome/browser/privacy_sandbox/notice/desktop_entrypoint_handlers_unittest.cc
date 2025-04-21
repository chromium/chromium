// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/notice/desktop_entrypoint_handlers.h"

#include "base/test/mock_callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace privacy_sandbox {
namespace {

using testing::Mock;

class PrivacySandboxNoticeEntryPointHandlersTest : public testing::Test {
 protected:
  base::MockCallback<base::RepeatingCallback<void()>>
      mock_entry_point_callback_;
  std::unique_ptr<NavigationHandler> handler_ =
      std::make_unique<NavigationHandler>(mock_entry_point_callback_.Get());
};

// Test that navigation alerts view manager.
TEST_F(PrivacySandboxNoticeEntryPointHandlersTest,
       TestNavigationCallsEntryPointCallback) {
  EXPECT_CALL(mock_entry_point_callback_, Run()).Times(1);
  handler_->HandleNewNavigation(nullptr, nullptr);
  Mock::VerifyAndClearExpectations(&mock_entry_point_callback_);
}

}  // namespace
}  // namespace privacy_sandbox
