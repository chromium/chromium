// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_ui_controller.h"

#include <memory>

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sharing/mock_sharing_service.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/sync/protocol/sync.pb.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kText[] = "Text to be copied";
const char kExpectedText[] = "Text to be copied";
const char kReceiverGuid[] = "test_receiver_guid";
const char kReceiverName[] = "test_receiver_name";

class SharedClipboardUiControllerTest : public testing::Test {
 public:
  SharedClipboardUiControllerTest() = default;

  void SetUp() override {
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    SharingServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<testing::NiceMock<MockSharingService>>();
        }));
    syncer::DeviceInfo device_info(kReceiverGuid, kReceiverName,
                                   "chrome_version", "user_agent",
                                   sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
                                   "device_id", base::SysInfo::HardwareInfo(),
                                   /*last_updated_timestamp=*/base::Time::Now(),
                                   /*send_tab_to_self_receiving_enabled=*/false,
                                   /*sharing_info=*/base::nullopt);
    controller_ = SharedClipboardUiController::GetOrCreateFromWebContents(
        web_contents_.get());
    controller_->OnDeviceSelected(base::UTF8ToUTF16(kText), device_info);
  }

 protected:
  testing::NiceMock<MockSharingService>* service() {
    return static_cast<testing::NiceMock<MockSharingService>*>(
        SharingServiceFactory::GetForBrowserContext(&profile_));
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  SharedClipboardUiController* controller_ = nullptr;
};
}  // namespace

MATCHER_P(ProtoEquals, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

// Check the call to sharing service when a device is chosen.
TEST_F(SharedClipboardUiControllerTest, OnDeviceChosen) {
  syncer::DeviceInfo device_info(kReceiverGuid, kReceiverName, "chrome_version",
                                 "user_agent",
                                 sync_pb::SyncEnums_DeviceType_TYPE_PHONE,
                                 "device_id", base::SysInfo::HardwareInfo(),
                                 /*last_updated_timestamp=*/base::Time::Now(),
                                 /*send_tab_to_self_receiving_enabled=*/false,
                                 /*sharing_info=*/base::nullopt);

  chrome_browser_sharing::SharingMessage sharing_message;
  sharing_message.mutable_shared_clipboard_message()->set_text(kExpectedText);
  EXPECT_CALL(*service(),
              SendMessageToDevice(testing::Eq(kReceiverGuid),
                                  testing::Eq(kSendMessageTimeout),
                                  ProtoEquals(sharing_message), testing::_));
  controller_->OnDeviceChosen(device_info);
}

// Check the call to sharing service to get all synced devices.
TEST_F(SharedClipboardUiControllerTest, GetSyncedDevices) {
  EXPECT_CALL(*service(),
              GetDeviceCandidates(testing::Eq(
                  sync_pb::SharingSpecificFields::SHARED_CLIPBOARD)));
  controller_->GetDevices();
}
