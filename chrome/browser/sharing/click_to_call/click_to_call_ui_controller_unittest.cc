// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_ui_controller.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/instance_id/instance_id_driver.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/mock_sharing_service.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sync/protocol/device_info_specifics.pb.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/weak_document_ptr.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::Eq;
using ::testing::Property;

namespace {

const char kPhoneNumber[] = "073%2099%209999%2099";
const char kReceiverGuid[] = "test_receiver_guid";
const char kReceiverName[] = "test_receiver_name";

class ClickToCallUiControllerTest : public testing::Test {
 public:
  ClickToCallUiControllerTest() = default;

  void SetUp() override {
    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    SharingServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<testing::NiceMock<MockSharingService>>();
        }));
    ClickToCallUiController::ShowDialog(
        web_contents_.get(),
        /*initiating_origin=*/std::nullopt,
        /*initiator_document=*/content::WeakDocumentPtr(),
        GURL(base::StrCat({"tel:", kPhoneNumber})), false, u"TestApp");
    controller_ = ClickToCallUiController::GetOrCreateFromWebContents(
        web_contents_.get());
  }

 protected:
  testing::NiceMock<MockSharingService>* service() {
    return static_cast<testing::NiceMock<MockSharingService>*>(
        SharingServiceFactory::GetForBrowserContext(&profile_));
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  raw_ptr<ClickToCallUiController> controller_ = nullptr;
};
}  // namespace

MATCHER_P(ProtoEquals, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

// Check the call to sharing service when a device is chosen.
TEST_F(ClickToCallUiControllerTest, OnDeviceChosen) {
  auto device_info = SharingTargetDeviceInfo(
      kReceiverGuid, kReceiverName, SharingDevicePlatform::kUnknown,
      /*pulse_interval=*/base::TimeDelta(),
      syncer::DeviceInfo::FormFactor::kUnknown,
      /*last_updated_timestamp=*/base::Time());

  components_sharing_message::SharingMessage sharing_message;
  sharing_message.mutable_click_to_call_message()->set_phone_number(
      kPhoneNumber);
  EXPECT_CALL(
      *service(),
      SendMessageToDevice(
          Property(&SharingTargetDeviceInfo::guid, kReceiverGuid),
          Eq(kSharingMessageTTL), ProtoEquals(sharing_message), testing::_));
  controller_->OnDeviceChosen(device_info);
}

// Check the call to sharing service to get all synced devices.
TEST_F(ClickToCallUiControllerTest, GetSyncedDevices) {
  EXPECT_CALL(*service(),
              GetDeviceCandidates(
                  Eq(sync_pb::SharingSpecificFields::CLICK_TO_CALL_V2)));
  controller_->GetDevices();
}
