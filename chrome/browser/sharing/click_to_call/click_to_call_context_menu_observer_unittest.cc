// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/click_to_call/click_to_call_context_menu_observer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/mock_render_view_context_menu.h"
#include "chrome/browser/sharing/click_to_call/click_to_call_utils.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "components/sharing_message/features.h"
#include "components/sharing_message/mock_sharing_service.h"
#include "components/sharing_message/sharing_constants.h"
#include "components/sharing_message/sharing_target_device_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::Return;

using SharingMessage = components_sharing_message::SharingMessage;

namespace {

const char kPhoneNumber[] = "+9876543210";

class ClickToCallContextMenuObserverTest : public testing::Test {
 public:
  ClickToCallContextMenuObserverTest() = default;

  ClickToCallContextMenuObserverTest(
      const ClickToCallContextMenuObserverTest&) = delete;
  ClickToCallContextMenuObserverTest& operator=(
      const ClickToCallContextMenuObserverTest&) = delete;

  ~ClickToCallContextMenuObserverTest() override = default;

  void SetUp() override {
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        menu_.GetBrowserContext(), nullptr);
    menu_.set_web_contents(web_contents_.get());
    SharingServiceFactory::GetInstance()->SetTestingFactory(
        menu_.GetBrowserContext(),
        base::BindRepeating([](content::BrowserContext* context)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<NiceMock<MockSharingService>>();
        }));
    observer_ = std::make_unique<ClickToCallContextMenuObserver>(&menu_);
    menu_.SetObserver(observer_.get());
  }

  void BuildMenu(const std::string& phone_number) {
    observer_->BuildMenu(phone_number, /*selection_text=*/std::string(),
                         SharingClickToCallEntryPoint::kRightClickLink);
    sharing_message.mutable_click_to_call_message()->set_phone_number(
        phone_number);
  }

  std::vector<SharingTargetDeviceInfo> CreateFakeDevices(int count) {
    std::vector<SharingTargetDeviceInfo> devices;
    for (int i = 0; i < count; i++) {
      devices.emplace_back(SharingTargetDeviceInfo(
          base::StrCat({"guid", base::NumberToString(i)}), "name",
          SharingDevicePlatform::kUnknown,
          /*pulse_interval=*/base::TimeDelta(),
          syncer::DeviceInfo::FormFactor::kUnknown,
          /*last_updated_timestamp=*/base::Time()));
    }
    return devices;
  }

 protected:
  NiceMock<MockSharingService>* service() {
    return static_cast<NiceMock<MockSharingService>*>(
        SharingServiceFactory::GetForBrowserContext(menu_.GetBrowserContext()));
  }

  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  MockRenderViewContextMenu menu_{/* incognito= */ false};
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<ClickToCallContextMenuObserver> observer_;
  SharingMessage sharing_message;
};

}  // namespace

MATCHER_P(ProtoEquals, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

TEST_F(ClickToCallContextMenuObserverTest, NoDevices_DoNotShowMenu) {
  auto devices = CreateFakeDevices(0);

  EXPECT_CALL(*service(), GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));

  BuildMenu(kPhoneNumber);

  EXPECT_EQ(0U, menu_.GetMenuSize());
}

TEST_F(ClickToCallContextMenuObserverTest, SingleDevice_ShowMenu) {
  auto devices = CreateFakeDevices(1);
  auto guid = devices[0].guid();

  EXPECT_CALL(*service(), GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));

  BuildMenu(kPhoneNumber);
  ASSERT_EQ(1U, menu_.GetMenuSize());

  MockRenderViewContextMenu::MockMenuItem item;
  ASSERT_TRUE(menu_.GetMenuItem(0, &item));
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
            item.command_id);

  // Emulate click on the device.
  EXPECT_CALL(*service(),
              SendMessageToDevice(
                  Property(&SharingTargetDeviceInfo::guid, guid),
                  Eq(kSharingMessageTTL), ProtoEquals(sharing_message), _))
      .Times(1);
  menu_.ExecuteCommand(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_SINGLE_DEVICE,
                       0);
}

TEST_F(ClickToCallContextMenuObserverTest, MultipleDevices_ShowMenu) {
  constexpr int device_count = 3;
  auto devices = CreateFakeDevices(device_count);
  std::vector<std::string> guids;
  for (const SharingTargetDeviceInfo& device : devices) {
    guids.push_back(device.guid());
  }

  EXPECT_CALL(*service(), GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));

  BuildMenu(kPhoneNumber);
  ASSERT_EQ(device_count + 1U, menu_.GetMenuSize());

  // Assert item ordering.
  MockRenderViewContextMenu::MockMenuItem item;
  ASSERT_TRUE(menu_.GetMenuItem(0, &item));
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES,
            item.command_id);

  for (int i = 0; i < device_count; i++) {
    ASSERT_TRUE(menu_.GetMenuItem(i + 1, &item));
    EXPECT_EQ(kSubMenuFirstDeviceCommandId + i, item.command_id);
  }

  // Emulate clicks on all commands to check for commands with no device
  // assigned.
  for (int i = 0; i < kMaxDevicesShown; i++) {
    if (i < device_count) {
      EXPECT_CALL(*service(),
                  SendMessageToDevice(
                      Property(&SharingTargetDeviceInfo::guid, guids[i]),
                      Eq(kSharingMessageTTL), ProtoEquals(sharing_message), _))
          .Times(1);
    } else {
      EXPECT_CALL(*service(), SendMessageToDevice(_, _, _, _)).Times(0);
    }
    observer_->sub_menu_delegate_.ExecuteCommand(
        kSubMenuFirstDeviceCommandId + i, 0);
  }
}

TEST_F(ClickToCallContextMenuObserverTest,
       MultipleDevices_MoreThanMax_ShowMenu) {
  int device_count = kMaxDevicesShown + 1;
  auto devices = CreateFakeDevices(device_count);
  std::vector<std::string> guids;
  for (const SharingTargetDeviceInfo& device : devices) {
    guids.push_back(device.guid());
  }

  EXPECT_CALL(*service(), GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));

  BuildMenu(kPhoneNumber);
  ASSERT_EQ(kMaxDevicesShown + 1U, menu_.GetMenuSize());

  // Assert item ordering.
  MockRenderViewContextMenu::MockMenuItem item;
  ASSERT_TRUE(menu_.GetMenuItem(0, &item));
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SHARING_CLICK_TO_CALL_MULTIPLE_DEVICES,
            item.command_id);

  for (int i = 0; i < kMaxDevicesShown; i++) {
    ASSERT_TRUE(menu_.GetMenuItem(i + 1, &item));
    EXPECT_EQ(kSubMenuFirstDeviceCommandId + i, item.command_id);
  }

  // Emulate clicks on all device commands to check for commands outside valid
  // range too.
  for (int i = 0; i < device_count; i++) {
    if (i < kMaxDevicesShown) {
      EXPECT_CALL(*service(),
                  SendMessageToDevice(
                      Property(&SharingTargetDeviceInfo::guid, guids[i]),
                      Eq(kSharingMessageTTL), ProtoEquals(sharing_message), _))
          .Times(1);
    } else {
      EXPECT_CALL(*service(), SendMessageToDevice(_, _, _, _)).Times(0);
    }
    observer_->sub_menu_delegate_.ExecuteCommand(
        kSubMenuFirstDeviceCommandId + i, 0);
  }
}
