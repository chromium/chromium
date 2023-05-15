// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler_desktop.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/uuid.h"
#include "chrome/browser/sharing/fake_device_info.h"
#include "chrome/browser/sharing/mock_sharing_device_source.h"
#include "chrome/browser/sharing/mock_sharing_service.h"
#include "chrome/browser/sharing/proto/shared_clipboard_message.pb.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_test_base.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync_device_info/device_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace {

const char kText[] = "clipboard text";
const char kEmptyDeviceName[] = "";
const char kDeviceNameInDeviceInfo[] = "DeviceNameInDeviceInfo";
const char16_t kDeviceNameInDeviceInfo16[] = u"DeviceNameInDeviceInfo";
const char kDeviceNameInMessage[] = "DeviceNameInMessage";
const char16_t kDeviceNameInMessage16[] = u"DeviceNameInMessage";

class SharedClipboardMessageHandlerTest : public SharedClipboardTestBase {
 public:
  SharedClipboardMessageHandlerTest() = default;

  SharedClipboardMessageHandlerTest(const SharedClipboardMessageHandlerTest&) =
      delete;
  SharedClipboardMessageHandlerTest& operator=(
      const SharedClipboardMessageHandlerTest&) = delete;

  ~SharedClipboardMessageHandlerTest() override = default;

  void SetUp() override {
    SharedClipboardTestBase::SetUp();
    ON_CALL(device_source_, IsReady()).WillByDefault(testing::Return(true));
    message_handler_ = std::make_unique<SharedClipboardMessageHandlerDesktop>(
        &device_source_, &profile_);
  }

  chrome_browser_sharing::SharingMessage CreateMessage(std::string guid,
                                                       std::string device_name,
                                                       std::string text) {
    chrome_browser_sharing::SharingMessage message =
        SharedClipboardTestBase::CreateMessage(guid, device_name);
    message.mutable_shared_clipboard_message()->set_text(text);
    return message;
  }

 protected:
  std::unique_ptr<SharedClipboardMessageHandlerDesktop> message_handler_;
  MockSharingDeviceSource device_source_;
};

}  // namespace

TEST_F(SharedClipboardMessageHandlerTest, NotificationWithoutDeviceName) {
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  {
    EXPECT_CALL(device_source_, GetDeviceByGuid(guid))
        .WillOnce(
            [](const std::string& guid) -> std::unique_ptr<syncer::DeviceInfo> {
              return nullptr;
            });
    base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
    EXPECT_CALL(done_callback, Run(testing::Eq(nullptr))).Times(1);
    message_handler_->OnMessage(CreateMessage(guid, kEmptyDeviceName, kText),
                                done_callback.Get());
  }
  EXPECT_EQ(GetClipboardText(), kText);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_TITLE_UNKNOWN_DEVICE),
      GetNotification().title());
}

TEST_F(SharedClipboardMessageHandlerTest,
       NotificationWithDeviceNameFromDeviceInfo) {
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  {
    EXPECT_CALL(device_source_, GetDeviceByGuid(guid))
        .WillOnce(
            [](const std::string& guid) -> std::unique_ptr<syncer::DeviceInfo> {
              return CreateFakeDeviceInfo(
                  base::Uuid::GenerateRandomV4().AsLowercaseString(),
                  kDeviceNameInDeviceInfo);
            });
    base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
    EXPECT_CALL(done_callback, Run(testing::Eq(nullptr))).Times(1);
    message_handler_->OnMessage(CreateMessage(guid, kEmptyDeviceName, kText),
                                done_callback.Get());
  }
  EXPECT_EQ(GetClipboardText(), kText);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_TITLE,
                kDeviceNameInDeviceInfo16),
            GetNotification().title());
}

TEST_F(SharedClipboardMessageHandlerTest,
       NotificationWithDeviceNameFromMessage) {
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  {
    EXPECT_CALL(device_source_, GetDeviceByGuid(guid))
        .WillOnce(
            [](const std::string& guid) -> std::unique_ptr<syncer::DeviceInfo> {
              return nullptr;
            });
    base::MockCallback<SharingMessageHandler::DoneCallback> done_callback;
    EXPECT_CALL(done_callback, Run(testing::Eq(nullptr))).Times(1);
    message_handler_->OnMessage(
        CreateMessage(guid, kDeviceNameInMessage, kText), done_callback.Get());
  }
  EXPECT_EQ(GetClipboardText(), kText);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_NOTIFICATION_TITLE,
                kDeviceNameInMessage16),
            GetNotification().title());
}
