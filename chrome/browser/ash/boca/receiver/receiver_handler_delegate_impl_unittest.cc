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
#include "chromeos/ash/components/policy/device_local_account/device_local_account_type.h"
#include "components/account_id/account_id.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/core/fake_session_manager_delegate.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/fake_user_manager_delegate.h"
#include "components/user_manager/scoped_user_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_manager_impl.h"
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
    user_manager_.Reset(std::make_unique<user_manager::UserManagerImpl>(
        std::make_unique<user_manager::FakeUserManagerDelegate>(),
        &testing_local_state_));
    user_manager::UserManager::RegisterPrefs(testing_local_state_.registry());
    account_id_ = user_manager::TestHelper(user_manager::UserManager::Get())
                      .AddKioskWebAppUser(GenerateDeviceLocalAccountUserId(
                          /*account_id=*/"webkiosk",
                          policy::DeviceLocalAccountType::kWebKioskApp))
                      ->GetAccountId();
    session_manager_.OnUserManagerCreated(user_manager::UserManager::Get());
  }

  void CreateSession() {
    session_manager_.CreateSession(account_id_, /*username_hash=*/"hash",
                                   /*new_user=*/false,
                                   /*has_active_session=*/false);
  }

  TestingPrefServiceSimple testing_local_state_;
  AccountId account_id_;
  MockKioskController kiosk_controller_;
  user_manager::ScopedUserManager user_manager_;
  session_manager::SessionManager session_manager_{
      std::make_unique<session_manager::FakeSessionManagerDelegate>()};
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
