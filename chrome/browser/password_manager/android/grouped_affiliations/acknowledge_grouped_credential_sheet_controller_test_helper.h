// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_GROUPED_AFFILIATIONS_ACKNOWLEDGE_GROUPED_CREDENTIAL_SHEET_CONTROLLER_TEST_HELPER_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_GROUPED_AFFILIATIONS_ACKNOWLEDGE_GROUPED_CREDENTIAL_SHEET_CONTROLLER_TEST_HELPER_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "chrome/browser/password_manager/android/grouped_affiliations/acknowledge_grouped_credential_sheet_bridge.h"
#include "chrome/browser/password_manager/android/grouped_affiliations/acknowledge_grouped_credential_sheet_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/android/window_android.h"

// The class is meant to help testing controllers owning the
// AcknowledgeGroupedCredentialSheetController. The test class should call
// `CreateController` first and own the unique ptr returned.
class AcknowledgeGroupedCredentialSheetControllerTestHelper {
 public:
  using JniDelegate = AcknowledgeGroupedCredentialSheetBridge::JniDelegate;

  class MockJniDelegate : public JniDelegate {
   public:
    MockJniDelegate();
    MockJniDelegate(const MockJniDelegate&) = delete;
    MockJniDelegate& operator=(const MockJniDelegate&) = delete;
    ~MockJniDelegate() override;

    MOCK_METHOD(void,
                Create,
                (const gfx::NativeWindow,
                 AcknowledgeGroupedCredentialSheetBridge*),
                (override));
    MOCK_METHOD(void, Show, (std::string, std::string), (override));
    MOCK_METHOD(void, Dismiss, (), (override));
  };

  AcknowledgeGroupedCredentialSheetControllerTestHelper();
  AcknowledgeGroupedCredentialSheetControllerTestHelper(
      const AcknowledgeGroupedCredentialSheetControllerTestHelper&) = delete;
  AcknowledgeGroupedCredentialSheetControllerTestHelper& operator=(
      const AcknowledgeGroupedCredentialSheetControllerTestHelper&) = delete;
  ~AcknowledgeGroupedCredentialSheetControllerTestHelper();

  std::unique_ptr<AcknowledgeGroupedCredentialSheetController>
  CreateController();

  void DismissSheet(bool accepted);

  MockJniDelegate* jni_bridge() { return jni_bridge_; }

 private:
  raw_ptr<AcknowledgeGroupedCredentialSheetBridge> bridge_;
  raw_ptr<MockJniDelegate> jni_bridge_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_GROUPED_AFFILIATIONS_ACKNOWLEDGE_GROUPED_CREDENTIAL_SHEET_CONTROLLER_TEST_HELPER_H_
