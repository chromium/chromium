// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/chromeos/extensions/users_private/users_private_delegate.h"
#include "chrome/browser/chromeos/extensions/users_private/users_private_delegate_factory.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/lock/screen_locker_tester.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/chromeos/settings/stub_cros_settings_provider.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/users_private.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/tpm/stub_install_attributes.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/ownership/mock_owner_key_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_utils.h"
#include "crypto/rsa_private_key.h"
#include "extensions/browser/api/test/test_api.h"
#include "extensions/common/switches.h"

namespace extensions {
namespace {

class TestPrefsUtil : public PrefsUtil {
 public:
  explicit TestPrefsUtil(Profile* profile) : PrefsUtil(profile) {}

  std::unique_ptr<api::settings_private::PrefObject> GetPref(
      const std::string& name) override {
    if (name != "cros.accounts.users")
      return PrefsUtil::GetPref(name);

    std::unique_ptr<api::settings_private::PrefObject> pref_object(
        new api::settings_private::PrefObject());
    pref_object->key = name;
    pref_object->type = api::settings_private::PrefType::PREF_TYPE_LIST;

    base::ListValue* value = new base::ListValue();
    for (auto& email : whitelisted_users_) {
      value->AppendString(email);
    }
    pref_object->value.reset(value);

    return pref_object;
  }

  bool AppendToListCrosSetting(const std::string& pref_name,
                               const base::Value& value) override {
    std::string email;
    value.GetAsString(&email);

    for (auto& user : whitelisted_users_) {
      if (email == user)
        return false;
    }

    whitelisted_users_.push_back(email);
    return true;
  }

  bool RemoveFromListCrosSetting(const std::string& pref_name,
                                 const base::Value& value) override {
    std::string email;
    value.GetAsString(&email);

    auto iter =
        std::find(whitelisted_users_.begin(), whitelisted_users_.end(), email);
    if (iter != whitelisted_users_.end())
      whitelisted_users_.erase(iter);

    return true;
  }

 private:
  std::vector<std::string> whitelisted_users_;
};

class TestDelegate : public UsersPrivateDelegate {
 public:
  explicit TestDelegate(Profile* profile) : UsersPrivateDelegate(profile) {
    profile_ = profile;
    prefs_util_ = nullptr;
  }
  ~TestDelegate() override = default;

  PrefsUtil* GetPrefsUtil() override {
    if (!prefs_util_)
      prefs_util_.reset(new TestPrefsUtil(profile_));

    return prefs_util_.get();
  }

 private:
  Profile* profile_;  // weak
  std::unique_ptr<TestPrefsUtil> prefs_util_;

  DISALLOW_COPY_AND_ASSIGN(TestDelegate);
};

class UsersPrivateApiTest : public ExtensionApiTest {
 public:
  UsersPrivateApiTest() {
    // Mock owner key pairs. Note this needs to happen before
    // OwnerSettingsServiceChromeOS is created.
    scoped_refptr<ownership::MockOwnerKeyUtil> owner_key_util =
        new ownership::MockOwnerKeyUtil();
    owner_key_util->SetPrivateKey(crypto::RSAPrivateKey::Create(512));

    chromeos::OwnerSettingsServiceChromeOSFactory::GetInstance()
        ->SetOwnerKeyUtilForTesting(owner_key_util);

    scoped_testing_cros_settings_.device_settings()->Set(
        chromeos::kDeviceOwner, base::Value("testuser@gmail.com"));
  }
  ~UsersPrivateApiTest() override = default;

  static std::unique_ptr<KeyedService> GetUsersPrivateDelegate(
      content::BrowserContext* profile) {
    CHECK(s_test_delegate_);
    return base::WrapUnique(s_test_delegate_);
  }

  void SetUpOnMainThread() override {
    ExtensionApiTest::SetUpOnMainThread();
    if (!s_test_delegate_)
      s_test_delegate_ = new TestDelegate(profile());

    UsersPrivateDelegateFactory::GetInstance()->SetTestingFactory(
        profile(),
        base::BindRepeating(&UsersPrivateApiTest::GetUsersPrivateDelegate));
    content::RunAllPendingInMessageLoop();
  }

 protected:
  bool RunSubtest(const std::string& subtest) {
    return RunExtensionSubtest("users_private", "main.html?" + subtest,
                               kFlagLoadAsComponent);
  }

  // Static pointer to the TestDelegate so that it can be accessed in
  // GetUsersPrivateDelegate() passed to SetTestingFactory().
  static TestDelegate* s_test_delegate_;

 private:
  chromeos::ScopedStubInstallAttributes scoped_stub_install_attributes_;
  chromeos::ScopedTestingCrosSettings scoped_testing_cros_settings_;

  DISALLOW_COPY_AND_ASSIGN(UsersPrivateApiTest);
};

// static
TestDelegate* UsersPrivateApiTest::s_test_delegate_ = NULL;

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
    test_config_.SetPath({"loginStatus", "isLoggedIn"}, base::Value(logged_in));
    test_config_.SetPath({"loginStatus", "isScreenLocked"},
                         base::Value(screen_locked));
  }

 private:
  base::DictionaryValue test_config_;
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

IN_PROC_BROWSER_TEST_F(UsersPrivateApiTest, IsWhitelistedUser) {
  EXPECT_TRUE(RunSubtest("isWhitelistedUser")) << message_;
}

IN_PROC_BROWSER_TEST_F(UsersPrivateApiTest, IsOwner) {
  EXPECT_TRUE(RunSubtest("isOwner")) << message_;
}

// User profile - logged in, screen not locked.
IN_PROC_BROWSER_TEST_F(UsersPrivateApiLoginStatusTest, User) {
  EXPECT_TRUE(RunExtensionSubtest("users_private", "main.html?getLoginStatus",
                                  kFlagLoadAsComponent))
      << message_;
}

// TODO(achuith): Signin profile - not logged in, screen not locked.

// Screenlock - logged in, screen locked.
IN_PROC_BROWSER_TEST_F(UsersPrivateApiLockStatusTest, ScreenLock) {
  chromeos::ScreenLockerTester().Lock();
  EXPECT_TRUE(RunExtensionSubtest("users_private", "main.html?getLoginStatus",
                                  kFlagLoadAsComponent))
      << message_;
}

}  // namespace extensions
