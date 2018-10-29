// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "apps/test/app_window_waiter.h"
#include "base/base64.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/fake_cws.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/app_launch_controller.h"
#include "chrome/browser/chromeos/ownership/owner_settings_service_chromeos_factory.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/policy/device_policy_builder.h"
#include "chrome/browser/chromeos/policy/device_policy_cros_browser_test.h"
#include "chrome/browser/chromeos/settings/stub_install_attributes.h"
#include "chrome/browser/extensions/browsertest_util.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chromeos/dbus/cryptohome_client.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/fake_session_manager_client.h"
#include "chromeos/dbus/shill_manager_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/browser/app_window/native_app_window.h"
#include "extensions/common/value_builder.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/cros_system_api/switches/chrome_switches.h"

namespace em = enterprise_management;

namespace chromeos {

namespace {

// This is a simple test app that creates an app window and immediately closes
// it again. Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/ggbflgnkafappblpkiflbgpmkfdpnhhe
constexpr char kTestKioskApp[] = "ggbflgnkafappblpkiflbgpmkfdpnhhe";

// This is a simple test that only sends an extension message when app launch is
// requested. Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/mogpelihofihkjnkkfkcchbkchggmcld
constexpr char kTestNonKioskEnabledApp[] = "mogpelihofihkjnkkfkcchbkchggmcld";

// Primary kiosk app that runs tests for chrome.management API.
// The tests are run on the kiosk app launch event.
// It has a secondary test kiosk app, which is loaded alongside the app. The
// secondary app will send a message to run chrome.management API tests in
// in its context as well.
// The app's CRX is located under:
//   chrome/test/data/chromeos/app_mode/webstore/downloads/
//       faiboenfkkoaedoehhkjmenkhidadgje.crx
// Source from which the CRX is generated is under path:
//   chrome/test/data/chromeos/app_mode/management_api/primary_app/
constexpr char kTestManagementApiKioskApp[] =
    "faiboenfkkoaedoehhkjmenkhidadgje";

// Secondary kiosk app that runs tests for chrome.management API.
// The app is loaded alongside |kTestManagementApiKioskApp|. The tests are run
// in the response to a message sent from |kTestManagementApiKioskApp|.
// The app's CRX is located under:
//   chrome/test/data/chromeos/app_mode/webstore/downloads/
//       lfaidgolgikbpapkmdhoppddflhaocnf.crx
// Source from which the CRX is generated is under path:
//   chrome/test/data/chromeos/app_mode/management_api/secondary_app/
constexpr char kTestManagementApiSecondaryApp[] =
    "lfaidgolgikbpapkmdhoppddflhaocnf";

constexpr char kTestAccountId[] = "enterprise-kiosk-app@localhost";

constexpr char kSessionManagerStateCache[] = "test_session_manager_state.json";

// Keys for values in dictionary used to preserve session manager state.
constexpr char kLoginArgsKey[] = "login_args";
constexpr char kExtraArgsKey[] = "extra_args";
constexpr char kArgNameKey[] = "name";
constexpr char kArgValueKey[] = "value";

// Default set policy switches.
constexpr struct {
  const char* name;
  const char* value;
} kDefaultPolicySwitches[] = {{"test_switch_1", ""},
                              {"test_switch_2", "test_switch_2_value"}};

// Fake session manager implementation that persists its state in local file.
// It can be used to preserve session state in PRE_ browser tests.
// Primarily used for testing user/login switches.
class PersistentSessionManagerClient : public FakeSessionManagerClient {
 public:
  PersistentSessionManagerClient() {}

  ~PersistentSessionManagerClient() override {
    PersistFlagsToFile(backing_file_);
  }

  // Initializes session state (primarily session flags)- if |backing_file|
  // exists, the session state is restored from the file value. Otherwise it's
  // set to the default session state.
  void Initialize(const base::FilePath& backing_file) {
    backing_file_ = backing_file;

    if (ExtractFlagsFromFile(backing_file_))
      return;

    // Failed to extract ached flags - set the default values.
    login_args_ = {{"login-manager", ""}};

    extra_args_ = {{switches::kPolicySwitchesBegin, ""}};
    for (size_t i = 0; i < arraysize(kDefaultPolicySwitches); ++i) {
      extra_args_.push_back(
          {kDefaultPolicySwitches[i].name, kDefaultPolicySwitches[i].value});
    }
    extra_args_.push_back({switches::kPolicySwitchesEnd, ""});
  }

  void AppendSwitchesToCommandLine(base::CommandLine* command_line) {
    for (const auto& flag : login_args_)
      command_line->AppendSwitchASCII(flag.name, flag.value);
    for (const auto& flag : extra_args_)
      command_line->AppendSwitchASCII(flag.name, flag.value);
  }

  void StartSession(
      const cryptohome::AccountIdentifier& cryptohome_id) override {
    FakeSessionManagerClient::StartSession(cryptohome_id);

    std::string user_id_hash =
        CryptohomeClient::GetStubSanitizedUsername(cryptohome_id);
    login_args_ = {{"login-user", cryptohome_id.account_id()},
                   {"login-profile", user_id_hash}};
  }

  void StopSession() override {
    FakeSessionManagerClient::StopSession();

    login_args_ = {{"login-manager", ""}};
  }

  bool SupportsRestartToApplyUserFlags() const override { return true; }

  void SetFlagsForUser(const cryptohome::AccountIdentifier& identification,
                       const std::vector<std::string>& flags) override {
    extra_args_.clear();
    FakeSessionManagerClient::SetFlagsForUser(identification, flags);

    std::vector<std::string> argv = {"" /* Empty program */};
    argv.insert(argv.end(), flags.begin(), flags.end());

    // Parse flag name-value pairs using command line initialization.
    base::CommandLine cmd_line(base::CommandLine::NO_PROGRAM);
    cmd_line.InitFromArgv(argv);

    for (const auto& flag : cmd_line.GetSwitches())
      extra_args_.push_back({flag.first, flag.second});
  }

 private:
  // Keeps information about a switch - its name and value.
  struct Switch {
    std::string name;
    std::string value;
  };

  bool ExtractFlagsFromFile(const base::FilePath& backing_file) {
    JSONFileValueDeserializer deserializer(backing_file);

    int error_code = 0;
    std::unique_ptr<base::Value> value =
        deserializer.Deserialize(&error_code, nullptr);
    if (error_code != JSONFileValueDeserializer::JSON_NO_ERROR)
      return false;

    std::unique_ptr<base::DictionaryValue> value_dict =
        base::DictionaryValue::From(std::move(value));
    CHECK(value_dict);

    CHECK(InitArgListFromCachedValue(*value_dict, kLoginArgsKey, &login_args_));
    CHECK(InitArgListFromCachedValue(*value_dict, kExtraArgsKey, &extra_args_));
    return true;
  }

  bool PersistFlagsToFile(const base::FilePath& backing_file) {
    base::DictionaryValue cached_state;
    cached_state.Set(kLoginArgsKey, GetArgListValue(login_args_));
    cached_state.Set(kExtraArgsKey, GetArgListValue(extra_args_));

    JSONFileValueSerializer serializer(backing_file);
    return serializer.Serialize(cached_state);
  }

  std::unique_ptr<base::ListValue> GetArgListValue(
      const std::vector<Switch>& args) {
    std::unique_ptr<base::ListValue> result(new base::ListValue());
    for (const auto& arg : args) {
      result->Append(extensions::DictionaryBuilder()
                         .Set(kArgNameKey, arg.name)
                         .Set(kArgValueKey, arg.value)
                         .Build());
    }
    return result;
  }

  bool InitArgListFromCachedValue(const base::DictionaryValue& cache_value,
                                  const std::string& list_key,
                                  std::vector<Switch>* arg_list_out) {
    arg_list_out->clear();
    const base::ListValue* arg_list_value;
    if (!cache_value.GetList(list_key, &arg_list_value))
      return false;
    for (size_t i = 0; i < arg_list_value->GetSize(); ++i) {
      const base::DictionaryValue* arg_value;
      if (!arg_list_value->GetDictionary(i, &arg_value))
        return false;
      Switch arg;
      if (!arg_value->GetStringASCII(kArgNameKey, &arg.name) ||
          !arg_value->GetStringASCII(kArgValueKey, &arg.value)) {
        return false;
      }
      arg_list_out->push_back(arg);
    }
    return true;
  }

  std::vector<Switch> login_args_;
  std::vector<Switch> extra_args_;

  base::FilePath backing_file_;

  DISALLOW_COPY_AND_ASSIGN(PersistentSessionManagerClient);
};

// Used to listen for app termination notification.
class TerminationObserver : public content::NotificationObserver {
 public:
  TerminationObserver() {
    registrar_.Add(this, chrome::NOTIFICATION_APP_TERMINATING,
                   content::NotificationService::AllSources());
  }
  ~TerminationObserver() override = default;

  // Whether app has been terminated - i.e. whether app termination notification
  // has been observed.
  bool terminated() const { return notification_seen_; }

 private:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    ASSERT_EQ(chrome::NOTIFICATION_APP_TERMINATING, type);
    notification_seen_ = true;
  }

  bool notification_seen_ = false;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(TerminationObserver);
};

}  // namespace

class AutoLaunchedKioskTest : public extensions::ExtensionApiTest {
 public:
  AutoLaunchedKioskTest()
      : install_attributes_(
            chromeos::StubInstallAttributes::CreateCloudManaged("domain.com",
                                                                "device_id")),
        fake_session_manager_(new PersistentSessionManagerClient()),
        fake_cws_(new FakeCWS) {
    set_chromeos_user_ = false;
  }

  ~AutoLaunchedKioskTest() override = default;

  virtual std::string GetTestAppId() const { return kTestKioskApp; }
  virtual std::vector<std::string> GetTestSecondaryAppIds() const {
    return std::vector<std::string>();
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    AppLaunchController::SkipSplashWaitForTesting();

    extensions::ExtensionApiTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    fake_cws_->Init(embedded_test_server());
    fake_cws_->SetUpdateCrx(GetTestAppId(), GetTestAppId() + ".crx", "1.0.0");
    std::vector<std::string> secondary_apps = GetTestSecondaryAppIds();
    for (const auto& secondary_app : secondary_apps)
      fake_cws_->SetUpdateCrx(secondary_app, secondary_app + ".crx", "1.0.0");
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
  }

  bool SetUpUserDataDirectory() override {
    InitDevicePolicy();

    base::FilePath user_data_path;
    if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_path)) {
      ADD_FAILURE() << "Unable to get used data dir";
      return false;
    }

    if (!CacheDevicePolicyToLocalState(user_data_path))
      return false;

    // Restore session_manager state and ensure session manager flags are
    // applied.
    fake_session_manager_->Initialize(
        user_data_path.Append(kSessionManagerStateCache));
    fake_session_manager_->AppendSwitchesToCommandLine(
        base::CommandLine::ForCurrentProcess());

    return true;
  }

  void SetUpInProcessBrowserTestFixture() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    fake_session_manager_->set_device_policy(
        device_policy_helper_.device_policy()->GetBlob());
    fake_session_manager_->set_device_local_account_policy(
        kTestAccountId, device_local_account_policy_.GetBlob());

    // Arbitrary non-empty state keys.
    fake_session_manager_->set_server_backed_state_keys({"1"});
    DBusThreadManager::GetSetterForTesting()->SetSessionManagerClient(
        std::move(fake_session_manager_));

    extensions::ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }

  void PreRunTestOnMainThread() override {
    termination_observer_.reset(new TerminationObserver());
    InProcessBrowserTest::PreRunTestOnMainThread();
  }

  void SetUpOnMainThread() override {
    extensions::browsertest_util::CreateAndInitializeLocalCache();

    embedded_test_server()->StartAcceptingConnections();

    extensions::ExtensionApiTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    termination_observer_.reset();

    extensions::ExtensionApiTest::TearDownOnMainThread();
  }

  void InitDevicePolicy() {
    device_policy_helper_.InstallOwnerKey();
    device_policy_helper_.MarkAsEnterpriseOwned();

    // Create device policy, and cache it to local state.
    em::DeviceLocalAccountsProto* const device_local_accounts =
        device_policy_helper_.device_policy()
            ->payload()
            .mutable_device_local_accounts();

    em::DeviceLocalAccountInfoProto* const account =
        device_local_accounts->add_account();
    account->set_account_id(kTestAccountId);
    account->set_type(em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_KIOSK_APP);
    account->mutable_kiosk_app()->set_app_id(GetTestAppId());

    device_local_accounts->set_auto_login_id(kTestAccountId);

    device_policy_helper_.device_policy()->Build();

    device_local_account_policy_.policy_data().set_username(kTestAccountId);
    device_local_account_policy_.policy_data().set_policy_type(
        policy::dm_protocol::kChromePublicAccountPolicyType);
    device_local_account_policy_.policy_data().set_settings_entity_id(
        kTestAccountId);
    device_local_account_policy_.Build();
  }

  bool CacheDevicePolicyToLocalState(const base::FilePath& user_data_path) {
    em::PolicyData policy_data;
    if (!device_policy_helper_.device_policy()->payload().SerializeToString(
            policy_data.mutable_policy_value())) {
      ADD_FAILURE() << "Failed to serialize device policy.";
      return false;
    }
    const std::string policy_data_str = policy_data.SerializeAsString();
    std::string policy_data_encoded;
    base::Base64Encode(policy_data_str, &policy_data_encoded);

    std::unique_ptr<base::DictionaryValue> local_state =
        extensions::DictionaryBuilder()
            .Set(prefs::kDeviceSettingsCache, policy_data_encoded)
            .Set("PublicAccounts",
                 extensions::ListBuilder().Append(GetTestAppUserId()).Build())
            .Build();
    local_state->SetKey(prefs::kOobeComplete, base::Value(true));

    JSONFileValueSerializer serializer(
        user_data_path.Append(chrome::kLocalStateFilename));
    if (!serializer.Serialize(*local_state)) {
      ADD_FAILURE() << "Failed to write local state.";
      return false;
    }
    return true;
  }

  const std::string GetTestAppUserId() const {
    return policy::GenerateDeviceLocalAccountUserId(
        kTestAccountId, policy::DeviceLocalAccount::TYPE_KIOSK_APP);
  }

  bool CloseAppWindow(const std::string& app_id) {
    Profile* const app_profile = ProfileManager::GetPrimaryUserProfile();
    if (!app_profile) {
      ADD_FAILURE() << "No primary (app) profile.";
      return false;
    }

    extensions::AppWindowRegistry* const app_window_registry =
        extensions::AppWindowRegistry::Get(app_profile);
    extensions::AppWindow* const window =
        apps::AppWindowWaiter(app_window_registry, app_id).Wait();
    if (!window) {
      ADD_FAILURE() << "No app window found for " << app_id << ".";
      return false;
    }

    window->GetBaseWindow()->Close();

    // Wait until the app terminates if it is still running.
    if (!app_window_registry->GetAppWindowsForApp(app_id).empty())
      RunUntilBrowserProcessQuits();
    return true;
  }

  bool IsKioskAppAutoLaunched(const std::string& app_id) {
    KioskAppManager::App app;
    if (!KioskAppManager::Get()->GetApp(app_id, &app)) {
      ADD_FAILURE() << "App " << app_id << " not found.";
      return false;
    }
    return app.was_auto_launched_with_zero_delay;
  }

  void ExpectCommandLineHasDefaultPolicySwitches(
      const base::CommandLine& cmd_line) {
    for (size_t i = 0u; i < arraysize(kDefaultPolicySwitches); ++i) {
      EXPECT_TRUE(cmd_line.HasSwitch(kDefaultPolicySwitches[i].name))
          << "Missing flag " << kDefaultPolicySwitches[i].name;
      EXPECT_EQ(kDefaultPolicySwitches[i].value,
                cmd_line.GetSwitchValueASCII(kDefaultPolicySwitches[i].name))
          << "Invalid value for switch " << kDefaultPolicySwitches[i].name;
    }
  }

 protected:
  std::unique_ptr<TerminationObserver> termination_observer_;

 private:
  chromeos::ScopedStubInstallAttributes install_attributes_;
  policy::UserPolicyBuilder device_local_account_policy_;
  policy::DevicePolicyCrosTestHelper device_policy_helper_;
  std::unique_ptr<PersistentSessionManagerClient> fake_session_manager_;
  std::unique_ptr<FakeCWS> fake_cws_;

  DISALLOW_COPY_AND_ASSIGN(AutoLaunchedKioskTest);
};

IN_PROC_BROWSER_TEST_F(AutoLaunchedKioskTest, PRE_CrashRestore) {
  // Verify that Chrome hasn't already exited, e.g. in order to apply user
  // session flags.
  ASSERT_FALSE(termination_observer_->terminated());

  // Set up default network connections, so tests think the device is online.
  DBusThreadManager::Get()
      ->GetShillManagerClient()
      ->GetTestInterface()
      ->SetupDefaultEnvironment();

  // Check that policy flags have not been lost.
  ExpectCommandLineHasDefaultPolicySwitches(
      *base::CommandLine::ForCurrentProcess());

  ExtensionTestMessageListener listener("appWindowLoaded", false);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  EXPECT_TRUE(IsKioskAppAutoLaunched(kTestKioskApp));

  ASSERT_TRUE(CloseAppWindow(kTestKioskApp));
}

IN_PROC_BROWSER_TEST_F(AutoLaunchedKioskTest, CrashRestore) {
  // Verify that Chrome hasn't already exited, e.g. in order to apply user
  // session flags.
  ASSERT_FALSE(termination_observer_->terminated());

  ExpectCommandLineHasDefaultPolicySwitches(
      *base::CommandLine::ForCurrentProcess());

  ExtensionTestMessageListener listener("appWindowLoaded", false);
  EXPECT_TRUE(listener.WaitUntilSatisfied());

  EXPECT_TRUE(IsKioskAppAutoLaunched(kTestKioskApp));

  ASSERT_TRUE(CloseAppWindow(kTestKioskApp));
}

// Used to test app auto-launch flow when the launched app is not kiosk enabled.
class AutoLaunchedNonKioskEnabledAppTest : public AutoLaunchedKioskTest {
 public:
  AutoLaunchedNonKioskEnabledAppTest() {}
  ~AutoLaunchedNonKioskEnabledAppTest() override = default;

  std::string GetTestAppId() const override { return kTestNonKioskEnabledApp; }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoLaunchedNonKioskEnabledAppTest);
};

IN_PROC_BROWSER_TEST_F(AutoLaunchedNonKioskEnabledAppTest, NotLaunched) {
  // Verify that Chrome hasn't already exited, e.g. in order to apply user
  // session flags.
  ASSERT_FALSE(termination_observer_->terminated());

  EXPECT_TRUE(IsKioskAppAutoLaunched(kTestNonKioskEnabledApp));

  ExtensionTestMessageListener listener("launchRequested", false);

  content::WindowedNotificationObserver termination_waiter(
      chrome::NOTIFICATION_APP_TERMINATING,
      content::NotificationService::AllSources());

  // Set up default network connections, so tests think the device is online.
  DBusThreadManager::Get()
      ->GetShillManagerClient()
      ->GetTestInterface()
      ->SetupDefaultEnvironment();

  // App launch should be canceled, and user session stopped.
  termination_waiter.Wait();

  EXPECT_FALSE(listener.was_satisfied());
  EXPECT_EQ(KioskAppLaunchError::NOT_KIOSK_ENABLED, KioskAppLaunchError::Get());
}

// Used to test management API availability in kiosk sessions.
class ManagementApiKioskTest : public AutoLaunchedKioskTest {
 public:
  ManagementApiKioskTest() {}
  ~ManagementApiKioskTest() override = default;

  // AutoLaunchedKioskTest:
  std::string GetTestAppId() const override {
    return kTestManagementApiKioskApp;
  }
  std::vector<std::string> GetTestSecondaryAppIds() const override {
    return {kTestManagementApiSecondaryApp};
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ManagementApiKioskTest);
};

IN_PROC_BROWSER_TEST_F(ManagementApiKioskTest, ManagementApi) {
  // Set up default network connections, so tests think the device is online.
  DBusThreadManager::Get()
      ->GetShillManagerClient()
      ->GetTestInterface()
      ->SetupDefaultEnvironment();

  // The tests expects to recieve two test result messages:
  //  * result for tests run by the secondary kiosk app.
  //  * result for tests run by the primary kiosk app.
  extensions::ResultCatcher catcher;
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

}  // namespace chromeos
