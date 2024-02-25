// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/permission_update_message_controller_android.h"

#include "base/android/jni_android.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/messages/android/mock_message_dispatcher_bridge.h"
#include "components/strings/grit/components_strings.h"

class PermissionUpdateMessageControllerAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PermissionUpdateMessageControllerAndroidTest() = default;
  ~PermissionUpdateMessageControllerAndroidTest() override = default;

  void SetUp() override;
  void TearDown() override;

  PermissionUpdateMessageController* GetController() { return controller_; }

  void ShowLocation(base::OnceCallback<void(bool)> callback) {
    EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
    GetController()->ShowMessageInternal(
        {}, {}, {}, IDR_ANDROID_INFOBAR_GEOLOCATION,
        IDS_MESSAGE_MISSING_LOCATION_PERMISSION_TITLE,
        IDS_MESSAGE_MISSING_LOCATION_PERMISSION_TEXT, std::move(callback));
  }

  void ShowMedia(base::OnceCallback<void(bool)> callback) {
    EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);
    GetController()->ShowMessageInternal(
        {}, {}, {}, IDR_ANDROID_INFOBAR_GEOLOCATION,
        IDS_MESSAGE_MISSING_MICROPHONE_CAMERA_PERMISSION_TITLE,
        IDS_MESSAGE_MISSING_MICROPHONE_CAMERA_PERMISSIONS_TEXT,
        std::move(callback));
  }

  void ShowDownload(base::OnceCallback<void(bool)> callback,
                    bool expected_enqueue) {
    if (expected_enqueue)
      EXPECT_CALL(message_dispatcher_bridge_, EnqueueMessage);

    GetController()->ShowMessageInternal(
        {}, {}, {}, IDR_ANDORID_MESSAGE_PERMISSION_STORAGE,
        IDS_MESSAGE_MISSING_STORAGE_ACCESS_PERMISSION_TITLE,
        IDS_MESSAGE_STORAGE_ACCESS_PERMISSION_TEXT, std::move(callback));
  }

  size_t GetMessageDelegatesSize() {
    return GetController()->message_delegates_.size();
  }

  void OnPermissionGranted(bool granted) {
    GetController()->message_delegates_.front()->OnPermissionResult(granted);
  }

  void SetPermissionResultTriggeredSynchronously() {
    GetController()->message_delegates_.front()->message_->SetActionClick(
        base::BindOnce(
            &PermissionUpdateMessageControllerAndroidTest::OnPermissionGranted,
            base::Unretained(this), false));
  }

  void DismissedByPrimaryAction() {
    messages::MessageDispatcherBridge::Get()->DismissMessage(
        GetController()->message_delegates_.front()->message_.get(),
        messages::DismissReason::PRIMARY_ACTION);
  }

  void ExpectDismiss(messages::DismissReason expected_reason) {
    EXPECT_CALL(message_dispatcher_bridge_, DismissMessage)
        .WillOnce([expected_reason](messages::MessageWrapper* message,
                                    messages::DismissReason dismiss_reason) {
          EXPECT_EQ(static_cast<int>(expected_reason),
                    static_cast<int>(dismiss_reason));
          message->HandleDismissCallback(base::android::AttachCurrentThread(),
                                         static_cast<int>(dismiss_reason));
        });
  }

 private:
  raw_ptr<PermissionUpdateMessageController> controller_;
  messages::MockMessageDispatcherBridge message_dispatcher_bridge_;
};

void PermissionUpdateMessageControllerAndroidTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  messages::MessageDispatcherBridge::SetInstanceForTesting(
      &message_dispatcher_bridge_);
  PermissionUpdateMessageController::CreateForWebContents(web_contents());
  controller_ =
      PermissionUpdateMessageController::FromWebContents(web_contents());
}

void PermissionUpdateMessageControllerAndroidTest::TearDown() {
  messages::MessageDispatcherBridge::SetInstanceForTesting(nullptr);
  ChromeRenderViewHostTestHarness::TearDown();
}

TEST_F(PermissionUpdateMessageControllerAndroidTest,
       OnMessageDismissedByPrimaryAction) {
  base::MockOnceCallback<void(bool)> mock_permission_update_callback1;
  base::MockOnceCallback<void(bool)> mock_permission_update_callback2;
  ShowLocation(mock_permission_update_callback1.Get());
  EXPECT_EQ(1u, GetMessageDelegatesSize());
  ShowMedia(mock_permission_update_callback2.Get());
  EXPECT_EQ(2u, GetMessageDelegatesSize());
  EXPECT_CALL(mock_permission_update_callback1, Run(false));

  // Message is dismissed first by primary action, and then permission update
  // callback is invoked. In this case, the dismiss reason should be
  // PRIMARY_ACTION.
  ExpectDismiss(messages::DismissReason::PRIMARY_ACTION);
  DismissedByPrimaryAction();
  OnPermissionGranted(false);
  EXPECT_EQ(1u, GetMessageDelegatesSize());
  EXPECT_CALL(mock_permission_update_callback2, Run(true));
  ExpectDismiss(messages::DismissReason::PRIMARY_ACTION);
  DismissedByPrimaryAction();
  OnPermissionGranted(true);
  EXPECT_EQ(0u, GetMessageDelegatesSize());
}

TEST_F(PermissionUpdateMessageControllerAndroidTest, OnPermissionResult) {
  // Permission update callback is invoked before dismiss callback is triggered.
  // In this case, dismiss callback is invoked when the controller is trying
  // to delete the message delegate. The dismiss reason should be UNKNOWN.
  base::MockOnceCallback<void(bool)> mock_permission_update_callback1;
  ShowLocation(mock_permission_update_callback1.Get());
  EXPECT_EQ(1u, GetMessageDelegatesSize());
  SetPermissionResultTriggeredSynchronously();
  EXPECT_CALL(mock_permission_update_callback1, Run(true));
  ExpectDismiss(messages::DismissReason::UNKNOWN);
  OnPermissionGranted(true);
  EXPECT_EQ(0u, GetMessageDelegatesSize());

  base::MockOnceCallback<void(bool)> mock_permission_update_callback2;
  ShowMedia(mock_permission_update_callback2.Get());
  EXPECT_EQ(1u, GetMessageDelegatesSize());
  SetPermissionResultTriggeredSynchronously();
  EXPECT_CALL(mock_permission_update_callback2, Run(false));
  ExpectDismiss(messages::DismissReason::UNKNOWN);
  OnPermissionGranted(false);
  EXPECT_EQ(0u, GetMessageDelegatesSize());
}

TEST_F(PermissionUpdateMessageControllerAndroidTest,
       OnEnqueuingDuplciatedMessage) {
  base::MockOnceCallback<void(bool)> mock_permission_update_callback1;
  base::MockOnceCallback<void(bool)> mock_permission_update_callback2;
  ShowDownload(mock_permission_update_callback1.Get(), true);
  EXPECT_EQ(1u, GetMessageDelegatesSize());
  ShowDownload(mock_permission_update_callback2.Get(), false);
  EXPECT_EQ(1u, GetMessageDelegatesSize());
  EXPECT_CALL(mock_permission_update_callback1, Run(false));
  EXPECT_CALL(mock_permission_update_callback2, Run(false));

  // Message is dismissed first by primary action, and then permission update
  // callback is invoked. In this case, the dismiss reason should be
  // PRIMARY_ACTION.
  ExpectDismiss(messages::DismissReason::PRIMARY_ACTION);
  DismissedByPrimaryAction();
  OnPermissionGranted(false);
  EXPECT_EQ(0u, GetMessageDelegatesSize());
}
