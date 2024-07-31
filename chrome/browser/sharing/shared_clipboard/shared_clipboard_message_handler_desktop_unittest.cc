// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_message_handler_desktop.h"

#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "base/uuid.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_test_base.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/sharing_message/fake_device_info.h"
#include "components/sharing_message/mock_sharing_device_source.h"
#include "components/sharing_message/mock_sharing_service.h"
#include "components/sharing_message/proto/shared_clipboard_message.pb.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "components/sync/protocol/sync_enums.pb.h"
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

  components_sharing_message::SharingMessage
  CreateMessage(std::string guid, std::string device_name, std::string text) {
    components_sharing_message::SharingMessage message =
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
        .WillOnce([](const std::string& guid)
                      -> std::optional<SharingTargetDeviceInfo> {
          return std::nullopt;
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
        .WillOnce([](const std::string& guid) {
          return SharingTargetDeviceInfo(
              base::Uuid::GenerateRandomV4().AsLowercaseString(),
              kDeviceNameInDeviceInfo, SharingDevicePlatform::kUnknown,
              /*pulse_interval=*/base::TimeDelta(),
              syncer::DeviceInfo::FormFactor::kUnknown,
              /*last_updated_timestamp=*/base::Time());
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
        .WillOnce([](const std::string& guid)
                      -> std::optional<SharingTargetDeviceInfo> {
          return std::nullopt;
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
