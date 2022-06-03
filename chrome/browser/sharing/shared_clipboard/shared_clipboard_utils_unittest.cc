// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_context_menu_observer.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/sharing/mock_sharing_service.h"
#include "chrome/browser/sharing/shared_clipboard/shared_clipboard_utils.h"
#include "chrome/browser/sharing/sharing_device_source.h"
#include "chrome/browser/sharing/sharing_fcm_handler.h"
#include "chrome/browser/sharing/sharing_fcm_sender.h"
#include "chrome/browser/sharing/sharing_handler_registry.h"
#include "chrome/browser/sharing/sharing_service.h"
#include "chrome/browser/sharing/sharing_service_factory.h"
#include "chrome/browser/sharing/sharing_sync_preference.h"
#include "chrome/browser/sharing/vapid_key_manager.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;

namespace {

const char16_t kText[] = u"Some text to copy to phone device.";

class MockSharingDeviceRegistration : public SharingDeviceRegistration {
 public:
  MockSharingDeviceRegistration()
      : SharingDeviceRegistration(/*pref_service=*/nullptr,
                                  /*sharing_sync_preference=*/nullptr,
                                  /*vapid_key_manager=*/nullptr,
                                  /*instance_id_driver=*/nullptr,
                                  /*sync_service=*/nullptr) {}

  MockSharingDeviceRegistration(const MockSharingDeviceRegistration&) = delete;
  MockSharingDeviceRegistration& operator=(
      const MockSharingDeviceRegistration&) = delete;

  ~MockSharingDeviceRegistration() override = default;

  MOCK_CONST_METHOD0(IsSharedClipboardSupported, bool());
};

class SharedClipboardUtilsTest : public testing::Test {
 public:
  SharedClipboardUtilsTest() = default;

  SharedClipboardUtilsTest(const SharedClipboardUtilsTest&) = delete;
  SharedClipboardUtilsTest& operator=(const SharedClipboardUtilsTest&) = delete;

  ~SharedClipboardUtilsTest() override = default;

  void SetUp() override {
    SharingServiceFactory::GetInstance()->SetTestingFactory(
        &profile_, base::BindRepeating(&SharedClipboardUtilsTest::CreateService,
                                       base::Unretained(this)));
  }

 protected:
  std::unique_ptr<KeyedService> CreateService(
      content::BrowserContext* context) {
    return create_service_ ? std::make_unique<MockSharingService>() : nullptr;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  bool create_service_ = true;
};

}  // namespace

// TODO(https://crbug.com/1311675): Remove this test and file once all the
// shared clipboard implementation is gone.
TEST_F(SharedClipboardUtilsTest, UIFlagDisabled_DoNotShowMenu) {
  EXPECT_FALSE(ShouldOfferSharedClipboard(&profile_, kText));
}
