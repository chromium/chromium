// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/ash/extensions/users_private/users_private_delegate.h"
#include "chrome/browser/ash/extensions/users_private/users_private_delegate_factory.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/users_private.h"
#include "chromeos/ash/components/install_attributes/stub_install_attributes.h"
#include "chromeos/ash/components/policy/device_policy/cached_device_policy_updater.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/common/switches.h"

namespace extensions {
namespace {

class TestPrefsUtil : public PrefsUtil {
 public:
  explicit TestPrefsUtil(Profile* profile) : PrefsUtil(profile) {}

  std::optional<api::settings_private::PrefObject> GetPref(
      const std::string& name) override {
    if (name != "cros.accounts.users")
      return PrefsUtil::GetPref(name);

    api::settings_private::PrefObject pref_object;
    pref_object.key = name;
    pref_object.type = api::settings_private::PrefType::kList;

    base::Value::List value;
    for (auto& email : user_list_) {
      value.Append(email);
    }
    pref_object.value = base::Value(std::move(value));

    return pref_object;
  }

  bool AppendToListCrosSetting(const std::string& pref_name,
                               const base::Value& value) override {
    std::string email;
    if (value.is_string())
      email = value.GetString();

    for (auto& user : user_list_) {
      if (email == user)
        return false;
    }

    user_list_.push_back(email);
    return true;
  }

  bool RemoveFromListCrosSetting(const std::string& pref_name,
                                 const base::Value& value) override {
    std::string email;
    if (value.is_string())
      email = value.GetString();

    auto iter = std::ranges::find(user_list_, email);
    if (iter != user_list_.end())
      user_list_.erase(iter);

    return true;
  }

 private:
  std::vector<std::string> user_list_;
};

class TestDelegate : public UsersPrivateDelegate {
 public:
  explicit TestDelegate(Profile* profile) : UsersPrivateDelegate(profile) {
    profile_ = profile;
    prefs_util_ = nullptr;
  }

  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;

  ~TestDelegate() override = default;

  PrefsUtil* GetPrefsUtil() override {
    if (!prefs_util_)
      prefs_util_ = std::make_unique<TestPrefsUtil>(profile_);

    return prefs_util_.get();
  }

 private:
  raw_ptr<Profile, LeakedDanglingUntriaged> profile_;  // weak
  std::unique_ptr<TestPrefsUtil> prefs_util_;
};

class UsersPrivateApiTest
    : public InProcessBrowserTestMixinHostSupport<ExtensionApiTest> {
 public:
  UsersPrivateApiTest() = default;
  UsersPrivateApiTest(const UsersPrivateApiTest&) = delete;
  UsersPrivateApiTest& operator=(const UsersPrivateApiTest&) = delete;
  ~UsersPrivateApiTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    InProcessBrowserTestMixinHostSupport<
        ExtensionApiTest>::SetUpInProcessBrowserTestFixture();

    policy::CachedDevicePolicyUpdater updater;
    updater.policy_data().set_username("testuser@gmail.com");
    updater.policy_data().set_management_mode(
        enterprise_management::PolicyData::LOCAL_OWNER);
    updater.Commit();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTestMixinHostSupport<ExtensionApiTest>::SetUpOnMainThread();

    UsersPrivateDelegateFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating([](content::BrowserContext* context)
                                           -> std::unique_ptr<KeyedService> {
          return std::make_unique<TestDelegate>(
              Profile::FromBrowserContext(context));
        }));
    content::RunAllPendingInMessageLoop();

    auto* owner_settings_service =
        ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(profile());
    base::test::TestFuture<bool> future;
    owner_settings_service->IsOwnerAsync(future.GetCallback());
    ASSERT_TRUE(future.Get());
  }

 protected:
  bool RunSubtest(const std::string& subtest) {
    const std::string extension_url = "main.html?" + subtest;
    return RunExtensionTest("users_private",
                            {.extension_url = extension_url.c_str()},
                            {.load_as_component = true});
  }

 private:
  ash::ScopedStubInstallAttributes scoped_stub_install_attributes_{
      ash::StubInstallAttributes::CreateConsumerOwned()};
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CONSUMER_OWNED};
};

class LoginStatusTestConfig {
 public:
  LoginStatusTestConfig() = default;
  ~LoginStatusTestConfig() = default;

  void Init() {
    extensions::TestGetConfigFunction::set_test_config_state(&test_config_);
  }
  void Reset() {
    extensions::TestGetConfigFunction::set_test_config_state(nullptr);
  }
  void SetConfig(bool logged_in, bool screen_locked) {
    test_config_.SetByDottedPath("loginStatus.isLoggedIn",
                                 base::Value(logged_in));
    test_config_.SetByDottedPath("loginStatus.isScreenLocked",
                                 base::Value(screen_locked));
  }

 private:
  base::Value::Dict test_config_;
};

class UsersPrivateApiLoginStatusTest : public ExtensionApiTest {
 protected:
  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    test_config_.Init();
    test_config_.SetConfig(true /*logged_in*/, false /*screen_locked*/);
  }

  void TearDownOnMainThread() override {
    ExtensionApiTest::TearDownOnMainThread();
    test_config_.Reset();
  }

  LoginStatusTestConfig test_config_;
};

class UsersPrivateApiLockStatusTest : public UsersPrivateApiLoginStatusTest {
 protected:
  void SetUpOnMainThread() override {
    UsersPrivateApiLoginStatusTest::SetUpOnMainThread();
    test_config_.SetConfig(true /*logged_in*/, true /*screen_locked*/);
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(UsersPrivateApiTest, AddUser) {
  EXPECT_TRUE(RunSubtest("addUser")) << message_;
}

IN_PROC_BROWSER_TEST_F(UsersPrivateApiTest, AddAndRemoveUsers) {
  EXPECT_TRUE(RunSubtest("addAndRemoveUsers")) << message_;
}

IN_PROC_BROWSER_TEST_F(UsersPrivateApiTest, IsUserInList) {
  EXPECT_TRUE(RunSubtest("isUserInList")) << message_;
}

IN_PROC_BROWSER_TEST_F(UsersPrivateApiTest, IsOwner) {
  EXPECT_TRUE(RunSubtest("isOwner")) << message_;
}

// User profile - logged in, screen not locked.
IN_PROC_BROWSER_TEST_F(UsersPrivateApiLoginStatusTest, User) {
  EXPECT_TRUE(RunExtensionTest("users_private",
                               {.extension_url = "main.html?getLoginStatus"},
                               {.load_as_component = true}))
      << message_;
}

// TODO(achuith): Signin profile - not logged in, screen not locked.

// Screenlock - logged in, screen locked.
IN_PROC_BROWSER_TEST_F(UsersPrivateApiLockStatusTest, ScreenLock) {
  ash::ScreenLockerTester().Lock();
  EXPECT_TRUE(RunExtensionTest("users_private",
                               {.extension_url = "main.html?getLoginStatus"},
                               {.load_as_component = true}))
      << message_;
}

}  // namespace extensions
