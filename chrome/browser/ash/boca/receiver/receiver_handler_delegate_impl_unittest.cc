// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/receiver/receiver_handler_delegate_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/ash/app_mode/fake_kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/test/test_user_session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/image/image_skia.h"

namespace ash::boca_receiver {
namespace {

class MockKioskController : public ash::FakeKioskController {
 public:
  MockKioskController() = default;
  ~MockKioskController() override = default;

  MOCK_METHOD(std::optional<KioskApp>,
              GetAppById,
              (const KioskAppId&),
              (const, override));
};

struct IsAppEnabledTestCase {
  std::string test_name;
  std::string app_url;
  GURL kiosk_url;
  bool expected_enabled;
};

class ReceiverHandlerDelegateImplIsAppEnabledTest
    : public testing::TestWithParam<IsAppEnabledTestCase> {
 protected:
  void SetUp() override {
    user_session_manager_ = std::make_unique<ash::test::TestUserSessionManager>(
        TestingBrowserProcess::GetGlobal()->local_state());
    user_manager::User* user = user_session_manager_->AddKioskWebAppUser(
        GenerateDeviceLocalAccountUserId(
            /*account_id=*/"webkiosk",
            policy::DeviceLocalAccountType::kWebKioskApp));
    ASSERT_TRUE(user);
    account_id_ = user->GetAccountId();
  }

  void TearDown() override { user_session_manager_.reset(); }

  void CreateSession() { user_session_manager_->LogIn(account_id_); }

  AccountId account_id_;
  MockKioskController kiosk_controller_;
  std::unique_ptr<ash::test::TestUserSessionManager> user_session_manager_;
};

TEST_F(ReceiverHandlerDelegateImplIsAppEnabledTest, AppMissing) {
  CreateSession();
  const KioskAppId kiosk_app_id = KioskAppId::ForWebApp(account_id_);
  ReceiverHandlerDelegateImpl receiver_handler_delegate(/*web_ui=*/nullptr);
  EXPECT_CALL(kiosk_controller_, GetAppById(kiosk_app_id))
      .WillOnce(testing::Return(std::nullopt));
  EXPECT_FALSE(receiver_handler_delegate.IsAppEnabled("chrome://test/"));
}

TEST_F(ReceiverHandlerDelegateImplIsAppEnabledTest, InactiveSession) {
  ReceiverHandlerDelegateImpl receiver_handler_delegate(/*web_ui=*/nullptr);
  EXPECT_CALL(kiosk_controller_, GetAppById(testing::_)).Times(0);
  EXPECT_FALSE(receiver_handler_delegate.IsAppEnabled("chrome://test/"));
}

TEST_P(ReceiverHandlerDelegateImplIsAppEnabledTest, KioskSession) {
  CreateSession();
  const KioskAppId kiosk_app_id = KioskAppId::ForWebApp(account_id_);
  ReceiverHandlerDelegateImpl receiver_handler_delegate(/*web_ui=*/nullptr);
  EXPECT_CALL(kiosk_controller_, GetAppById(kiosk_app_id))
      .WillOnce(testing::Return(KioskApp{kiosk_app_id,
                                         /*name=*/"test-app-name",
                                         /*icon=*/gfx::ImageSkia(),
                                         GetParam().kiosk_url}));
  EXPECT_EQ(receiver_handler_delegate.IsAppEnabled(GetParam().app_url),
            GetParam().expected_enabled);
}

INSTANTIATE_TEST_SUITE_P(
    IsAppEnabledTests,
    ReceiverHandlerDelegateImplIsAppEnabledTest,
    testing::ValuesIn<IsAppEnabledTestCase>({
        {"SameUrl", "chrome://test", GURL("chrome://test/"), true},
        {"DifferentUrl", "chrome://test", GURL("chrome://test2"), false},
    }),
    [](const testing::TestParamInfo<
        ReceiverHandlerDelegateImplIsAppEnabledTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace
}  // namespace ash::boca_receiver
