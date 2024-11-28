// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/grouped_affiliations/acknowledge_grouped_credential_sheet_controller.h"

#include <memory>

#include "base/android/jni_android.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/password_manager/android/grouped_affiliations/acknowledge_grouped_credential_sheet_bridge.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/android/window_android.h"

using DismissReson = AcknowledgeGroupedCredentialSheetBridge::DismissReason;

namespace {
const char kCurrentHostname[] = "current.com";
const char kCredentialHostname[] = "credential.com";

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
  MOCK_METHOD((void),
              Show,
              (const std::string& current_hostname,
               const std::string& credential_hostname),
              (override));
  MOCK_METHOD((void), Dismiss, (), (override));
};
}  // namespace

class AcknowledgeGroupedCredentialSheetControllerTest : public testing::Test {
 public:
  AcknowledgeGroupedCredentialSheetControllerTest() {
    window_android_ = ui::WindowAndroid::CreateForTesting();
    auto mock_jni_bridge = std::make_unique<MockJniDelegate>();
    mock_jni_bridge_ = mock_jni_bridge.get();
    auto bridge = std::make_unique<AcknowledgeGroupedCredentialSheetBridge>(
        base::PassKey<class AcknowledgeGroupedCredentialSheetControllerTest>(),
        std::move(mock_jni_bridge));
    bridge_ = bridge.get();
    controller_ = std::make_unique<AcknowledgeGroupedCredentialSheetController>(
        base::PassKey<class AcknowledgeGroupedCredentialSheetControllerTest>(),
        std::move(bridge));
  }

  MockJniDelegate* mock_jni_bridge() { return mock_jni_bridge_; }

  AcknowledgeGroupedCredentialSheetBridge* bridge() { return bridge_; }

  std::unique_ptr<AcknowledgeGroupedCredentialSheetController> controller_;

  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting>
      window_android_;

 private:
  raw_ptr<MockJniDelegate> mock_jni_bridge_;
  raw_ptr<AcknowledgeGroupedCredentialSheetBridge> bridge_;
};

TEST_F(AcknowledgeGroupedCredentialSheetControllerTest,
       ShowAndDismissAcknowledgeSheet) {
  // TODO(crbug.com/372635361): After implementing the bridge, expect the call
  // to show the actual sheet. Now only checks that the callback is called.
  base::MockCallback<base::OnceCallback<void(DismissReson)>> mock_reply;
  EXPECT_CALL(*mock_jni_bridge(), Show(kCurrentHostname, kCredentialHostname));
  controller_->ShowAcknowledgeSheet(kCurrentHostname, kCredentialHostname,
                                    window_android_.get()->get(),
                                    mock_reply.Get());

  EXPECT_CALL(mock_reply, Run(DismissReson::kBack));
  bridge()->OnDismissed(jni_zero::AttachCurrentThread(),
                        /*accepted=*/static_cast<int>(DismissReson::kBack));
}

TEST_F(AcknowledgeGroupedCredentialSheetControllerTest,
       SheetDismissesWhenControllerIsDestroyed) {
  // TODO(crbug.com/372635361): After implementing the bridge, expect the call
  // to show the actual sheet. Now only checks that the callback is called.
  base::MockCallback<base::OnceCallback<void(DismissReson)>> mock_reply;
  EXPECT_CALL(*mock_jni_bridge(), Show);
  controller_->ShowAcknowledgeSheet(kCurrentHostname, kCredentialHostname,
                                    window_android_.get()->get(),
                                    mock_reply.Get());

  EXPECT_CALL(mock_reply, Run).Times(0);
  EXPECT_CALL(*mock_jni_bridge(), Dismiss);
  controller_.reset();
}
