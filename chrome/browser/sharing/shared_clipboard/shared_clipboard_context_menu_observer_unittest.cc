// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_context_menu_observer.h"

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/renderer_context_menu/mock_render_view_context_menu.h"
#include "chrome/browser/sharing/mock_sharing_service.h"
#include "chrome/browser/sharing/shared_clipboard/feature_flags.h"
#include "chrome/browser/sharing/sharing_constants.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "components/sync_device_info/device_info.h"
#include "content/public/common/context_menu_params.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;

using SharingMessage = chrome_browser_sharing::SharingMessage;

namespace {

const char kText[] = "Some random text to be copied.";

constexpr int kSeparatorCommandId = -1;

class SharedClipboardContextMenuObserverTest : public testing::Test {
 public:
  SharedClipboardContextMenuObserverTest() = default;

  ~SharedClipboardContextMenuObserverTest() override = default;

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
    observer_ = std::make_unique<SharedClipboardContextMenuObserver>(&menu_);
    menu_.SetObserver(observer_.get());
  }

  void InitMenu(const base::string16 text) {
    content::ContextMenuParams params;
    params.selection_text = text;
    observer_->InitMenu(params);
    sharing_message.mutable_shared_clipboard_message()->set_text(
        base::UTF16ToUTF8(text));
  }

  std::vector<std::unique_ptr<syncer::DeviceInfo>> CreateMockDevices(
      int count) {
    std::vector<std::unique_ptr<syncer::DeviceInfo>> devices;
    for (int i = 0; i < count; i++) {
      devices.emplace_back(std::make_unique<syncer::DeviceInfo>(
          base::StrCat({"guid", base::NumberToString(i)}), "name",
          "chrome_version", "user_agent",
          sync_pb::SyncEnums_DeviceType_TYPE_PHONE, "device_id",
          base::SysInfo::HardwareInfo(),
          /*last_updated_timestamp=*/base::Time::Now(),
          /*send_tab_to_self_receiving_enabled=*/false,
          /*sharing_info=*/base::nullopt));
    }
    return devices;
  }

 protected:
  NiceMock<MockSharingService>* service() {
    return static_cast<NiceMock<MockSharingService>*>(
        SharingServiceFactory::GetForBrowserContext(menu_.GetBrowserContext()));
  }

  content::BrowserTaskEnvironment task_environment_;
  MockRenderViewContextMenu menu_{/* incognito= */ false};
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<SharedClipboardContextMenuObserver> observer_;
  SharingMessage sharing_message;

  DISALLOW_COPY_AND_ASSIGN(SharedClipboardContextMenuObserverTest);
};

}  // namespace

MATCHER_P(ProtoEquals, message, "") {
  std::string expected_serialized, actual_serialized;
  message.SerializeToString(&expected_serialized);
  arg.SerializeToString(&actual_serialized);
  return expected_serialized == actual_serialized;
}

TEST_F(SharedClipboardContextMenuObserverTest, NoDevices_DoNotShowMenu) {
  auto devices = CreateMockDevices(0);

  EXPECT_CALL(*service(), GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));

  InitMenu(base::ASCIIToUTF16(kText));

  EXPECT_EQ(0U, menu_.GetMenuSize());
}

TEST_F(SharedClipboardContextMenuObserverTest, SingleDevice_ShowMenu) {
  auto devices = CreateMockDevices(1);
  auto guid = devices[0]->guid();

  EXPECT_CALL(*service(), GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));

  InitMenu(base::ASCIIToUTF16(kText));

  // The first item is a separator and the second item is the device.
  EXPECT_EQ(2U, menu_.GetMenuSize());

  // Assert item ordering.
  MockRenderViewContextMenu::MockMenuItem item;
  ASSERT_TRUE(menu_.GetMenuItem(0, &item));
  EXPECT_EQ(kSeparatorCommandId, item.command_id);

  ASSERT_TRUE(menu_.GetMenuItem(1, &item));
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE,
            item.command_id);

  // Emulate click on the device.
  EXPECT_CALL(*service(), SendMessageToDevice(Eq(guid), Eq(kSendMessageTimeout),
                                              ProtoEquals(sharing_message), _))
      .Times(1);
  menu_.ExecuteCommand(
      IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_SINGLE_DEVICE, 0);
}

TEST_F(SharedClipboardContextMenuObserverTest, MultipleDevices_ShowMenu) {
  constexpr int device_count = 3;
  auto devices = CreateMockDevices(device_count);
  std::vector<std::string> guids;
  for (auto& device : devices)
    guids.push_back(device->guid());

  EXPECT_CALL(*service(), GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));

  InitMenu(base::ASCIIToUTF16(kText));

  EXPECT_EQ(device_count + 2U, menu_.GetMenuSize());

  // Assert item ordering.
  MockRenderViewContextMenu::MockMenuItem item;
  ASSERT_TRUE(menu_.GetMenuItem(0, &item));
  EXPECT_EQ(kSeparatorCommandId, item.command_id);

  ASSERT_TRUE(menu_.GetMenuItem(1, &item));
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_MULTIPLE_DEVICES,
            item.command_id);

  for (int i = 0; i < device_count; i++) {
    ASSERT_TRUE(menu_.GetMenuItem(i + 2, &item));
    EXPECT_EQ(kSubMenuFirstDeviceCommandId + i, item.command_id);
  }

  // Emulate clicks on all commands to check for commands with no device
  // assigned.
  for (int i = 0; i < kMaxDevicesShown; i++) {
    if (i < device_count) {
      EXPECT_CALL(*service(),
                  SendMessageToDevice(Eq(guids[i]), Eq(kSendMessageTimeout),
                                      ProtoEquals(sharing_message), _))
          .Times(1);
    } else {
      EXPECT_CALL(*service(), SendMessageToDevice(_, _, _, _)).Times(0);
    }
    observer_->sub_menu_delegate_.ExecuteCommand(
        kSubMenuFirstDeviceCommandId + i, 0);
  }
}

TEST_F(SharedClipboardContextMenuObserverTest,
       MultipleDevices_MoreThanMax_ShowMenu) {
  int device_count = kMaxDevicesShown + 1;
  auto devices = CreateMockDevices(device_count);
  std::vector<std::string> guids;
  for (auto& device : devices)
    guids.push_back(device->guid());

  EXPECT_CALL(*service(), GetDeviceCandidates(_))
      .WillOnce(Return(ByMove(std::move(devices))));

  InitMenu(base::ASCIIToUTF16(kText));

  EXPECT_EQ(kMaxDevicesShown + 2U, menu_.GetMenuSize());

  // Assert item ordering.
  MockRenderViewContextMenu::MockMenuItem item;
  ASSERT_TRUE(menu_.GetMenuItem(0, &item));
  EXPECT_EQ(kSeparatorCommandId, item.command_id);

  ASSERT_TRUE(menu_.GetMenuItem(1, &item));
  EXPECT_EQ(IDC_CONTENT_CONTEXT_SHARING_SHARED_CLIPBOARD_MULTIPLE_DEVICES,
            item.command_id);

  for (int i = 0; i < kMaxDevicesShown; i++) {
    ASSERT_TRUE(menu_.GetMenuItem(i + 2, &item));
    EXPECT_EQ(kSubMenuFirstDeviceCommandId + i, item.command_id);
  }

  // Emulate clicks on all device commands to check for commands outside valid
  // range too.
  for (int i = 0; i < device_count; i++) {
    if (i < kMaxDevicesShown) {
      EXPECT_CALL(*service(),
                  SendMessageToDevice(Eq(guids[i]), Eq(kSendMessageTimeout),
                                      ProtoEquals(sharing_message), _))
          .Times(1);
    } else {
      EXPECT_CALL(*service(), SendMessageToDevice(_, _, _, _)).Times(0);
    }
    observer_->sub_menu_delegate_.ExecuteCommand(
        kSubMenuFirstDeviceCommandId + i, 0);
  }
}
