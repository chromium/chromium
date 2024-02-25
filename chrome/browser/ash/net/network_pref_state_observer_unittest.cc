// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/network_pref_state_observer.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/proxy/ui_proxy_config_service.h"
#include "components/onc/onc_constants.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

namespace {

const char kUserId[] = "test@example.com";
const char kNetworkId[] = "wifi1_guid";  // Matches FakeShillManagerClient

}  // namespace

class NetworkPrefStateObserverTest : public testing::Test {
 public:
  NetworkPrefStateObserverTest()
      : fake_user_manager_(new FakeChromeUserManager),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_.get())),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}

  NetworkPrefStateObserverTest(const NetworkPrefStateObserverTest&) = delete;
  NetworkPrefStateObserverTest& operator=(const NetworkPrefStateObserverTest&) =
      delete;

  ~NetworkPrefStateObserverTest() override {}

  void SetUp() override {
    testing::Test::SetUp();
    ASSERT_TRUE(profile_manager_.SetUp());
    network_pref_state_observer_ = std::make_unique<NetworkPrefStateObserver>();
  }

  void TearDown() override {
    network_pref_state_observer_.reset();
    testing::Test::TearDown();
  }

 protected:
  Profile* LoginAndReturnProfile() {
    AccountId account_id = AccountId::FromUserEmail(kUserId);
    fake_user_manager_->AddUser(account_id);
    fake_user_manager_->LoginUser(account_id);
    Profile* profile = profile_manager_.CreateTestingProfile(kUserId);
    session_manager_.NotifyUserProfileLoaded(account_id);
    base::RunLoop().RunUntilIdle();
    return profile;
  }

  content::BrowserTaskEnvironment task_environment_;
  NetworkHandlerTestHelper network_handler_test_helper_;
  raw_ptr<FakeChromeUserManager, DanglingUntriaged> fake_user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
  TestingProfileManager profile_manager_;
  session_manager::SessionManager session_manager_;
  std::unique_ptr<NetworkPrefStateObserver> network_pref_state_observer_;
};

TEST_F(NetworkPrefStateObserverTest, LoginUser) {
  // UIProxyConfigService should exist with device PrefService.
  UIProxyConfigService* device_ui_proxy_config_service =
      NetworkHandler::GetUiProxyConfigService();
  ASSERT_TRUE(device_ui_proxy_config_service);
  // There should be no proxy config available.
  base::Value::Dict ui_proxy_config;
  EXPECT_FALSE(device_ui_proxy_config_service->MergeEnforcedProxyConfig(
      kNetworkId, &ui_proxy_config));

  Profile* profile = LoginAndReturnProfile();

  // New UIProxyConfigService should be created with a profile PrefService.
  UIProxyConfigService* profile_ui_proxy_config_service =
      NetworkHandler::GetUiProxyConfigService();
  ASSERT_TRUE(profile_ui_proxy_config_service);
  ASSERT_NE(device_ui_proxy_config_service, profile_ui_proxy_config_service);
  ui_proxy_config = base::Value::Dict();
  EXPECT_FALSE(profile_ui_proxy_config_service->MergeEnforcedProxyConfig(
      kNetworkId, &ui_proxy_config));

  // Set the profile pref to PAC script mode.
  auto proxy_config = base::Value::Dict()
                          .Set("mode", ProxyPrefs::kPacScriptProxyModeName)
                          .Set("pac_url", "http://proxy");
  profile->GetPrefs()->Set(proxy_config::prefs::kProxy,
                           base::Value(std::move(proxy_config)));
  base::RunLoop().RunUntilIdle();

  // Mode should now be MODE_PAC_SCRIPT.
  ui_proxy_config.clear();
  EXPECT_TRUE(
      NetworkHandler::GetUiProxyConfigService()->MergeEnforcedProxyConfig(
          kNetworkId, &ui_proxy_config));
  base::Value* mode = ui_proxy_config.FindByDottedPath(base::JoinString(
      {::onc::network_config::kType, ::onc::kAugmentationActiveSetting}, "."));
  ASSERT_TRUE(mode);
  EXPECT_EQ(base::Value(::onc::proxy::kPAC), *mode);
}

}  // namespace ash
