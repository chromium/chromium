// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/grouped_affiliations/acknowledge_grouped_credential_sheet_controller.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/password_manager/android/grouped_affiliations/acknowledge_grouped_credential_sheet_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
class MockJniDelegate
    : public AcknowledgeGroupedCredentialSheetBridge::JniDelegate {
 public:
  MockJniDelegate() = default;
  ~MockJniDelegate() override = default;

  MOCK_METHOD((void),
              Create,
              (const gfx::NativeWindow,
               AcknowledgeGroupedCredentialSheetBridge*),
              (override));
  MOCK_METHOD((void), Show, (), (override));
  MOCK_METHOD((void), Dismiss, (), (override));
};
}  // namespace

class AcknowledgeGroupedCredentialSheetControllerTest : public testing::Test {
 public:
  AcknowledgeGroupedCredentialSheetControllerTest() {
    auto mock_jni_bridge = std::make_unique<MockJniDelegate>();
    mock_jni_bridge_ = mock_jni_bridge.get();
    controller_ = std::make_unique<AcknowledgeGroupedCredentialSheetController>(
        std::make_unique<AcknowledgeGroupedCredentialSheetBridge>(
            base::PassKey<
                class AcknowledgeGroupedCredentialSheetControllerTest>(),
            std::move(mock_jni_bridge)));
  }
  AcknowledgeGroupedCredentialSheetController* GetController() {
    return controller_.get();
  }

  MockJniDelegate* mock_jni_bridge() { return mock_jni_bridge_; }

 private:
  std::unique_ptr<AcknowledgeGroupedCredentialSheetController> controller_;
  raw_ptr<MockJniDelegate> mock_jni_bridge_;
};

TEST_F(AcknowledgeGroupedCredentialSheetControllerTest, ShowAcknowledgeSheet) {
  // TODO(crbug.com/372635361): After implementing the bridge, expect the call
  // to show the actual sheet. Now only checks that the callback is called.
  base::MockCallback<base::OnceCallback<void(bool)>> mock_reply;
  EXPECT_CALL(*mock_jni_bridge(), Show);
  GetController()->ShowAcknowledgeSheet(mock_reply.Get());
}
