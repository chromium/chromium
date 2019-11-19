// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/network_pref_state_observer.h"

#include <memory>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/proxy/ui_proxy_config_service.h"
#include "components/onc/onc_constants.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace {
const char kUserId[] = "test@example.com";
const char kNetworkId[] = "wifi1_guid";  // Matches FakeShillManagerClient
}

namespace chromeos {

class NetworkPrefStateObserverTest : public testing::Test {
 public:
  NetworkPrefStateObserverTest()
      : fake_user_manager_(new FakeChromeUserManager),
        user_manager_enabler_(base::WrapUnique(fake_user_manager_)),
        profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~NetworkPrefStateObserverTest() override {}

  void SetUp() override {
    testing::Test::SetUp();
    DBusThreadManager::Initialize();
    NetworkHandler::Initialize();
    base::RunLoop().RunUntilIdle();
    ASSERT_TRUE(profile_manager_.SetUp());
    network_pref_state_observer_.reset(new NetworkPrefStateObserver);
  }

  void TearDown() override {
    network_pref_state_observer_.reset();
    NetworkHandler::Shutdown();
    DBusThreadManager::Shutdown();
    testing::Test::TearDown();
  }

 protected:
  Profile* LoginAndReturnProfile() {
    fake_user_manager_->AddUser(AccountId::FromUserEmail(kUserId));
    fake_user_manager_->LoginUser(AccountId::FromUserEmail(kUserId));
    Profile* profile = profile_manager_.CreateTestingProfile(kUserId);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
        content::NotificationService::AllSources(),
        content::Details<Profile>(profile));
    base::RunLoop().RunUntilIdle();
    return profile;
  }

  content::BrowserTaskEnvironment task_environment_;
  FakeChromeUserManager* fake_user_manager_;
  user_manager::ScopedUserManager user_manager_enabler_;
  TestingProfileManager profile_manager_;
  std::unique_ptr<NetworkPrefStateObserver> network_pref_state_observer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkPrefStateObserverTest);
};

TEST_F(NetworkPrefStateObserverTest, LoginUser) {
  // UIProxyConfigService should exist with device PrefService.
  UIProxyConfigService* device_ui_proxy_config_service =
      NetworkHandler::Get()->ui_proxy_config_service();
  ASSERT_TRUE(device_ui_proxy_config_service);
  // There should be no proxy config available.
  base::Value ui_proxy_config(base::Value::Type::DICTIONARY);
  EXPECT_FALSE(device_ui_proxy_config_service->MergeEnforcedProxyConfig(
      kNetworkId, &ui_proxy_config));

  Profile* profile = LoginAndReturnProfile();

  // New UIProxyConfigService should be created with a profile PrefService.
  UIProxyConfigService* profile_ui_proxy_config_service =
      NetworkHandler::Get()->ui_proxy_config_service();
  ASSERT_TRUE(profile_ui_proxy_config_service);
  ASSERT_NE(device_ui_proxy_config_service, profile_ui_proxy_config_service);
  ui_proxy_config = base::Value(base::Value::Type::DICTIONARY);
  EXPECT_FALSE(profile_ui_proxy_config_service->MergeEnforcedProxyConfig(
      kNetworkId, &ui_proxy_config));

  // Set the profile pref to PAC script mode.
  std::unique_ptr<base::DictionaryValue> proxy_config(
      std::make_unique<base::DictionaryValue>());
  proxy_config->SetString("mode", ProxyPrefs::kPacScriptProxyModeName);
  proxy_config->SetString("pac_url", "http://proxy");
  profile->GetPrefs()->Set(proxy_config::prefs::kProxy, *proxy_config.get());
  base::RunLoop().RunUntilIdle();

  // Mode should now be MODE_PAC_SCRIPT.
  ui_proxy_config = base::Value(base::Value::Type::DICTIONARY);
  EXPECT_TRUE(NetworkHandler::Get()
                  ->ui_proxy_config_service()
                  ->MergeEnforcedProxyConfig(kNetworkId, &ui_proxy_config));
  base::Value* mode = ui_proxy_config.FindPath(
      {::onc::network_config::kType, ::onc::kAugmentationActiveSetting});
  ASSERT_TRUE(mode);
  EXPECT_EQ(base::Value(::onc::proxy::kPAC), *mode);
}

}  // namespace chromeos
