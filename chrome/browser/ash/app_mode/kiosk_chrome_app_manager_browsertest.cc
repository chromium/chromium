// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/app_mode/kiosk_chrome_app_manager.h"

#include <stddef.h>

#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/test/gtest_tags.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/repeating_test_future.h"
#include "base/test/scoped_chromeos_version_info.h"
#include "base/test/test_future.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/consumer_kiosk_test_helper.h"
#include "chrome/browser/ash/app_mode/fake_cws.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data.h"
#include "chrome/browser/ash/app_mode/kiosk_app_data_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/ash/ownership/fake_owner_settings_service.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_local_account.h"
#include "chrome/browser/ash/policy/enrollment/enrollment_config.h"
#include "chrome/browser/ash/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/extensions/webstore_data_fetcher.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/crx_file/crx_verifier.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/core/common/device_local_account_type.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/sandboxed_unpacker.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/manifest.mojom-shared.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::test::RepeatingTestFuture;
using base::test::TestFuture;

namespace ash {

namespace {

// An app to test local fs data persistence across app update. V1 app writes
// data into local fs. V2 app reads and verifies the data.
// Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/abbjjkefakmllanciinhgjgjamdmlbdg
// The version 1.0.0 installed is in
//   chrome/test/data/chromeos/app_mode/webstore/downloads/
//       abbjjkefakmllanciinhgjgjamdmlbdg.crx
// The version 2.0.0 crx is in
//   chrome/test/data/chromeos/app_mode/webstore/downloads/
//       abbjjkefakmllanciinhgjgjamdmlbdg_v2_read_and_verify_data.crx
const char kTestLocalFsKioskApp[] = "abbjjkefakmllanciinhgjgjamdmlbdg";
const char kTestLocalFsKioskAppName[] = "Kiosk App With Local Data";

scoped_refptr<extensions::Extension> MakeKioskApp(
    const std::string& name,
    const std::string& version,
    const std::string& id,
    const std::string& required_platform_version) {
  auto value = base::Value::Dict()
                   .Set("name", name)
                   .Set("version", version)
                   .SetByDottedPath("app.background.scripts",
                                    base::Value::List().Append("main.js"))
                   .Set("kiosk_enabled", true);
  if (!required_platform_version.empty()) {
    value.SetByDottedPath("kiosk.required_platform_version",
                          required_platform_version);
  }

  std::string err;
  scoped_refptr<extensions::Extension> app = extensions::Extension::Create(
      base::FilePath(), extensions::mojom::ManifestLocation::kInternal, value,
      extensions::Extension::WAS_INSTALLED_BY_DEFAULT, id, &err);
  EXPECT_EQ(err, "");
  return app;
}

class AppDataLoadWaiter : public KioskAppManagerObserver {
 public:
  explicit AppDataLoadWaiter(KioskChromeAppManager* manager)
      : manager_(manager) {
    manager_->AddObserver(this);
  }
  AppDataLoadWaiter(const AppDataLoadWaiter&) = delete;
  AppDataLoadWaiter& operator=(const AppDataLoadWaiter&) = delete;
  ~AppDataLoadWaiter() override { manager_->RemoveObserver(this); }

  void Wait(int count) {
    for (int i = 0; i < count; i++) {
      app_data_load_waiter_.Take();
    }
  }

  void Reset() {
    data_change_count_ = 0;
    data_load_failure_count_ = 0;
    while (!app_data_load_waiter_.IsEmpty()) {
      app_data_load_waiter_.Take();
    }
  }

  int data_change_count() const { return data_change_count_; }
  int data_load_failure_count() const { return data_load_failure_count_; }

 private:
  // KioskAppManagerObserver overrides:
  void OnKioskAppDataChanged(const std::string& app_id) override {
    data_change_count_++;
    app_data_load_waiter_.AddValue(true);
  }

  void OnKioskAppDataLoadFailure(const std::string& app_id) override {
    data_load_failure_count_++;
    app_data_load_waiter_.AddValue(false);
  }

  void OnKioskExtensionLoadedInCache(const std::string& app_id) override {
    OnKioskAppDataChanged(app_id);
  }

  void OnKioskExtensionDownloadFailed(const std::string& app_id) override {
    // Intentionally nothing to do here. Most tests which use this helper don't
    // care about extension downloading, only about fetching its app data. Also
    // fake_cws()->SetNoUpdate creates extension which will fail to download due
    // to missing update URL in manifest.
  }

  RepeatingTestFuture<bool> app_data_load_waiter_;
  raw_ptr<KioskChromeAppManager> manager_;
  int data_change_count_ = 0;
  int data_load_failure_count_ = 0;
};

}  // namespace

class ChromeAppKioskAppManagerTest : public InProcessBrowserTest {
 public:
  ChromeAppKioskAppManagerTest()
      : settings_helper_(false),
        fake_cws_(new FakeCWS()),
        verifier_format_override_(crx_file::VerifierFormat::CRX3) {}
  ChromeAppKioskAppManagerTest(const ChromeAppKioskAppManagerTest&) = delete;
  ChromeAppKioskAppManagerTest& operator=(const ChromeAppKioskAppManagerTest&) =
      delete;
  ~ChromeAppKioskAppManagerTest() override = default;

  // InProcessBrowserTest overrides:
  void SetUp() override {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);

    // Log the response code for WebstoreDataFetcher instance if it is not 200.
    // TODO(crbug.com/325314721): Use a mock FetchItemSnippetResponse instead
    // when the old item JSON API used for fetching webstore data is removed.
    extensions::WebstoreDataFetcher::SetLogResponseCodeForTesting(true);

    // Don't spin up the IO thread yet since no threads are allowed while
    // spawning sandbox host process. See crbug.com/322732.
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Initialize fake_cws_ to setup web store gallery.
    fake_cws_->Init(embedded_test_server());
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    // Start the accept thread as the sandbox host process has already been
    // spawned.
    embedded_test_server()->StartAcceptingConnections();

    settings_helper_.ReplaceDeviceSettingsProviderWithStub();
    owner_settings_service_ =
        settings_helper_.CreateOwnerSettingsService(browser()->profile());
  }

  void TearDownOnMainThread() override {
    owner_settings_service_.reset();
    settings_helper_.RestoreRealDeviceSettingsProvider();
  }

  std::string GetAppIds() const {
    KioskChromeAppManager::AppList apps = manager()->GetApps();

    std::string str;
    for (size_t i = 0; i < apps.size(); ++i) {
      if (i > 0) {
        str += ',';
      }
      str += apps[i].app_id;
    }

    return str;
  }

  // Locks device for enterprise.
  InstallAttributes::LockResult LockDeviceForEnterprise() {
    TestFuture<InstallAttributes::LockResult> result_future;
    policy::BrowserPolicyConnectorAsh* connector =
        g_browser_process->platform_part()->browser_policy_connector_ash();
    connector->GetInstallAttributes()->LockDevice(
        policy::DEVICE_MODE_ENTERPRISE, "domain.com",
        std::string(),  // realm
        "device-id", result_future.GetCallback());
    return result_future.Get();
  }

  void SetExistingApp(const std::string& app_id,
                      const std::string& app_name,
                      const std::string& icon_file_name,
                      const std::string& required_platform_version) {
    base::FilePath test_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    base::FilePath data_dir = test_dir.AppendASCII("chromeos/app_mode/");

    // Copy the icon file to temp dir for using because ClearAppData test
    // deletes it.
    base::FilePath icon_path =
        CopyFileToTempDir(data_dir.AppendASCII(icon_file_name));

    PrefService* local_state = g_browser_process->local_state();
    ScopedDictPrefUpdate dict_update(
        local_state, KioskChromeAppManager::kKioskDictionaryName);
    dict_update->Set(
        KioskAppDataBase::kKeyApps,
        base::Value::Dict()
            .SetByDottedPath(app_id + ".name", app_name)
            .SetByDottedPath(app_id + ".icon", icon_path.MaybeAsASCII())
            .SetByDottedPath(app_id + ".required_platform_version",
                             required_platform_version));

    // Make the app appear in device settings.
    auto device_local_accounts = base::Value::List().Append(
        base::Value::Dict()
            // Fake an account id. Note this needs to match
            // GenerateKioskAppAccountId in kiosk_chrome_app_manager.cc to make
            // SetAutoLaunchApp work with the existing app entry created here.
            .Set(kAccountsPrefDeviceLocalAccountsKeyId, app_id + "@kiosk-apps")
            .Set(kAccountsPrefDeviceLocalAccountsKeyType,
                 static_cast<int>(policy::DeviceLocalAccountType::kKioskApp))
            .Set(kAccountsPrefDeviceLocalAccountsKeyEphemeralMode,
                 static_cast<int>(
                     policy::DeviceLocalAccount::EphemeralMode::kUnset))
            .Set(kAccountsPrefDeviceLocalAccountsKeyKioskAppId, app_id));
    owner_settings_service_->Set(kAccountsPrefDeviceLocalAccounts,
                                 base::Value(std::move(device_local_accounts)));
  }

  bool GetCachedCrx(const std::string& app_id,
                    base::FilePath* file_path,
                    std::string* version) {
    return manager()->GetCachedCrx(app_id, file_path, version);
  }

  void UpdateAppsFromPolicy() { manager()->UpdateAppsFromPolicy(); }

  void CheckAppData(const std::string& app_id,
                    const std::string& expected_app_name,
                    const std::string& expected_required_platform_version) {
    // Check manifest data is cached correctly.
    KioskChromeAppManager::AppList apps = manager()->GetApps();
    ASSERT_EQ(1u, apps.size());
    EXPECT_EQ(app_id, apps[0].app_id);
    EXPECT_EQ(expected_app_name, apps[0].name);
    EXPECT_FALSE(apps[0].icon.size().IsEmpty());
    EXPECT_EQ(expected_required_platform_version,
              apps[0].required_platform_version);
  }

  void CheckAppDataAndCache(
      const std::string& app_id,
      const std::string& expected_app_name,
      const std::string& expected_required_platform_version) {
    CheckAppData(app_id, expected_app_name, expected_required_platform_version);

    // Check data is cached in local state correctly.
    PrefService* local_state = g_browser_process->local_state();
    const base::Value::Dict& dict =
        local_state->GetDict(KioskChromeAppManager::kKioskDictionaryName);

    const std::string name_key = "apps." + app_id + ".name";
    const std::string* name = dict.FindStringByDottedPath(name_key);
    ASSERT_TRUE(name);
    EXPECT_EQ(expected_app_name, *name);

    const std::string icon_path_key = "apps." + app_id + ".icon";
    const std::string* icon_path_string =
        dict.FindStringByDottedPath(icon_path_key);
    ASSERT_TRUE(icon_path_string);

    const std::string required_platform_version_key =
        "apps." + app_id + ".required_platform_version";
    const std::string* required_platform_version =
        dict.FindStringByDottedPath(required_platform_version_key);
    ASSERT_TRUE(required_platform_version);
    EXPECT_EQ(expected_required_platform_version, *required_platform_version);

    base::FilePath expected_icon_path;
    manager()->GetKioskAppIconCacheDir(&expected_icon_path);
    expected_icon_path =
        expected_icon_path.AppendASCII(app_id).AddExtension(".png");
    EXPECT_EQ(expected_icon_path.value(), *icon_path_string);
  }

  void RunAddNewAppTest(const std::string& id,
                        const std::string& expected_version,
                        const std::string& expected_app_name,
                        const std::string& expected_required_platform_version) {
    std::string crx_file_name = id + ".crx";
    fake_cws_->SetUpdateCrx(id, crx_file_name, expected_version);

    AppDataLoadWaiter waiter(manager());
    AddConsumerKioskChromeAppForTesting(
        CHECK_DEREF(owner_settings_service_.get()), id);
    waiter.Wait(3);
    EXPECT_EQ(waiter.data_change_count(), 3);
    EXPECT_EQ(waiter.data_load_failure_count(), 0);

    // Check CRX file is cached.
    base::FilePath crx_path;
    std::string crx_version;
    EXPECT_TRUE(GetCachedCrx(id, &crx_path, &crx_version));
    {
      base::ScopedAllowBlockingForTesting allow_io;
      EXPECT_TRUE(base::PathExists(crx_path));
    }
    EXPECT_EQ(expected_version, crx_version);
    // Verify the original crx file is identical to the cached file.
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    std::string src_file_path_str =
        std::string("chromeos/app_mode/webstore/downloads/") + crx_file_name;
    base::FilePath src_file_path = test_data_dir.Append(src_file_path_str);
    {
      base::ScopedAllowBlockingForTesting allow_io;
      EXPECT_TRUE(base::PathExists(src_file_path));
    }
    EXPECT_TRUE(base::ContentsEqual(src_file_path, crx_path));

    CheckAppDataAndCache(id, expected_app_name,
                         expected_required_platform_version);
  }

  // Copies the given file into temp dir and returns the full path
  // of the copied file.
  base::FilePath CopyFileToTempDir(const base::FilePath& file) {
    base::FilePath target_file = temp_dir_.GetPath().Append(file.BaseName());

    base::ScopedAllowBlockingForTesting allow_io;
    CHECK(base::CopyFile(file, target_file));
    return target_file;
  }

  KioskAppData* GetAppDataMutable(const std::string& app_id) {
    return manager()->GetAppDataMutable(app_id);
  }

  KioskChromeAppManager* manager() const {
    return KioskChromeAppManager::Get();
  }
  FakeCWS* fake_cws() { return fake_cws_.get(); }

 protected:
  ScopedCrosSettingsTestHelper settings_helper_;
  std::unique_ptr<FakeOwnerSettingsService> owner_settings_service_;

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<FakeCWS> fake_cws_;
  extensions::SandboxedUnpacker::ScopedVerifierFormatOverrideForTest
      verifier_format_override_;
};

IN_PROC_BROWSER_TEST_F(ChromeAppKioskAppManagerTest, Basic) {
  // Add a couple of apps. Use "fake_app_x" that do not have data on the test
  // server to avoid pending data loads that could be lingering on tear down and
  // cause DCHECK failure in utility_process_host.cc.
  AddConsumerKioskChromeAppForTesting(
      CHECK_DEREF(owner_settings_service_.get()), "fake_app_1");
  AddConsumerKioskChromeAppForTesting(
      CHECK_DEREF(owner_settings_service_.get()), "fake_app_2");
  EXPECT_EQ("fake_app_1,fake_app_2", GetAppIds());

  // Set an auto launch app.
  SetConsumerKioskAutoLaunchChromeAppForTesting(
      CHECK_DEREF(manager()), CHECK_DEREF(owner_settings_service_.get()),
      "fake_app_1");
  EXPECT_EQ("fake_app_1", manager()->GetAutoLaunchApp());

  // Make sure that if an app was auto launched with zero delay, it is reflected
  // in the app data.
  KioskChromeAppManager::App app;
  manager()->GetApp("fake_app_1", &app);
  EXPECT_FALSE(app.was_auto_launched_with_zero_delay);

  manager()->SetAppWasAutoLaunchedWithZeroDelay("fake_app_1");
  manager()->GetApp("fake_app_1", &app);
  EXPECT_TRUE(app.was_auto_launched_with_zero_delay);

  // Clear the auto launch app.
  SetConsumerKioskAutoLaunchChromeAppForTesting(
      CHECK_DEREF(manager()), CHECK_DEREF(owner_settings_service_.get()), "");
  EXPECT_EQ("", manager()->GetAutoLaunchApp());

  // App should still report it was auto launched with zero delay, even though
  // it is no longer set to auto launch in the future.
  manager()->GetApp("fake_app_1", &app);
  EXPECT_TRUE(app.was_auto_launched_with_zero_delay);

  // Set another auto launch app.
  SetConsumerKioskAutoLaunchChromeAppForTesting(
      CHECK_DEREF(manager()), CHECK_DEREF(owner_settings_service_.get()),
      "fake_app_2");
  EXPECT_EQ("fake_app_2", manager()->GetAutoLaunchApp());

  // Remove the auto launch app.
  RemoveConsumerKioskChromeAppForTesting(
      CHECK_DEREF(manager()), CHECK_DEREF(owner_settings_service_.get()),
      "fake_app_2");
  EXPECT_EQ("fake_app_1", GetAppIds());
  EXPECT_EQ("", manager()->GetAutoLaunchApp());

  // Add the just removed auto launch app again and it should no longer be
  // the auto launch app.
  AddConsumerKioskChromeAppForTesting(
      CHECK_DEREF(owner_settings_service_.get()), "fake_app_2");
  EXPECT_EQ("", manager()->GetAutoLaunchApp());
  RemoveConsumerKioskChromeAppForTesting(
      CHECK_DEREF(manager()), CHECK_DEREF(owner_settings_service_.get()),
      "fake_app_2");
  EXPECT_EQ("fake_app_1", GetAppIds());

  // Set a none exist app as auto launch.
  SetConsumerKioskAutoLaunchChromeAppForTesting(
      CHECK_DEREF(manager()), CHECK_DEREF(owner_settings_service_.get()),
      "none_exist_app");
  EXPECT_EQ("", manager()->GetAutoLaunchApp());

  // Add an existing app again.
  AddConsumerKioskChromeAppForTesting(
      CHECK_DEREF(owner_settings_service_.get()), "fake_app_1");
  EXPECT_EQ("fake_app_1", GetAppIds());
}

IN_PROC_BROWSER_TEST_F(ChromeAppKioskAppManagerTest, LoadCached) {
  SetExistingApp("app_1", "Cached App1 Name", "red16x16.png", "1234");

  fake_cws()->SetNoUpdate("app_1");
  AppDataLoadWaiter waiter(manager());
  waiter.Wait(1);
  EXPECT_EQ(waiter.data_change_count(), 1);
  EXPECT_EQ(waiter.data_load_failure_count(), 0);

  CheckAppData("app_1", "Cached App1 Name", "1234");
}

IN_PROC_BROWSER_TEST_F(ChromeAppKioskAppManagerTest, ClearAppData) {
  SetExistingApp("app_1", "Cached App1 Name", "red16x16.png", "");

  PrefService* local_state = g_browser_process->local_state();
  const base::Value::Dict& dict =
      local_state->GetDict(KioskChromeAppManager::kKioskDictionaryName);
  const base::Value::Dict* apps_dict =
      dict.FindDict(KioskAppDataBase::kKeyApps);
  EXPECT_TRUE(apps_dict);
  EXPECT_TRUE(apps_dict->contains("app_1"));

  manager()->ClearAppData("app_1");

  EXPECT_FALSE(apps_dict->contains("app_1"));
}

IN_PROC_BROWSER_TEST_F(ChromeAppKioskAppManagerTest, UpdateAppDataFromProfile) {
  SetExistingApp("app_1", "Cached App1 Name", "red16x16.png", "");

  fake_cws()->SetNoUpdate("app_1");
  AppDataLoadWaiter waiter(manager());
  waiter.Wait(1);
  EXPECT_EQ(waiter.data_change_count(), 1);
  EXPECT_EQ(waiter.data_load_failure_count(), 0);

  CheckAppData("app_1", "Cached App1 Name", "");

  scoped_refptr<extensions::Extension> updated_app =
      MakeKioskApp("Updated App1 Name", "2.0", "app_1", "1234");
  manager()->UpdateAppDataFromProfile("app_1", browser()->profile(),
                                      updated_app.get());

  waiter.Reset();
  waiter.Wait(1);
  EXPECT_EQ(waiter.data_change_count(), 1);
  EXPECT_EQ(waiter.data_load_failure_count(), 0);

  CheckAppData("app_1", "Updated App1 Name", "1234");
}

// Flaky; https://crbug.com/783450
IN_PROC_BROWSER_TEST_F(ChromeAppKioskAppManagerTest, UpdateAppDataFromCrx) {
  const char kAppId[] = "iiigpodgfihagabpagjehoocpakbnclp";
  const char kAppName[] = "Test Kiosk App";

  SetExistingApp(kAppId, kAppName, "red16x16.png", "");
  fake_cws()->SetNoUpdate(kAppId);
  AppDataLoadWaiter waiter(manager());
  waiter.Wait(1);
  EXPECT_EQ(waiter.data_change_count(), 1);
  EXPECT_EQ(waiter.data_load_failure_count(), 0);

  CheckAppData(kAppId, kAppName, "");

  // Fake app data load failure so that the manager will attempt to
  // load it from crx.
  KioskAppData* app_data = GetAppDataMutable(kAppId);
  app_data->SetStatusForTest(KioskAppData::Status::kError);

  // Copy test crx file to temp dir because the cache moves the file.
  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  base::FilePath data_dir =
      test_dir.AppendASCII("chromeos/app_mode/webstore/downloads/");
  base::FilePath crx_file = data_dir.AppendASCII(
      "iiigpodgfihagabpagjehoocpakbnclp_v2_required_platform_version_added."
      "crx");
  crx_file = CopyFileToTempDir(crx_file);

  TestFuture<const std::string&, bool> result_future;
  manager()->PutValidatedExternalExtension(kAppId, crx_file, "2.0.0",
                                           result_future.GetCallback());
  ASSERT_TRUE(result_future.Get<1>());

  // Wait for 3 data loaded events at the most. One for crx putting into cache,
  // one for update check and one for app data is updated from crx.
  const size_t kMaxDataChange = 3;
  for (size_t i = 0; i < kMaxDataChange &&
                     app_data->status() != KioskAppData::Status::kLoaded;
       ++i) {
    waiter.Reset();
    waiter.Wait(1);
    EXPECT_EQ(waiter.data_change_count(), 1);
    EXPECT_EQ(waiter.data_load_failure_count(), 0);
  }
  ASSERT_EQ(KioskAppData::Status::kLoaded, app_data->status());

  CheckAppData(kAppId, kAppName, "1234");
}

IN_PROC_BROWSER_TEST_F(ChromeAppKioskAppManagerTest, FailedToLoadFromCrx) {
  const char kAppId[] = "iiigpodgfihagabpagjehoocpakbnclp";
  const char kAppName[] = "Test Kiosk App";

  SetExistingApp(kAppId, kAppName, "red16x16.png", "");
  fake_cws()->SetNoUpdate(kAppId);
  AppDataLoadWaiter waiter(manager());
  waiter.Wait(1);
  EXPECT_EQ(waiter.data_change_count(), 1);
  EXPECT_EQ(waiter.data_load_failure_count(), 0);

  CheckAppData(kAppId, kAppName, "");

  // Fake app data load failure so that the manager will attempt to
  // load it from crx.
  KioskAppData* app_data = GetAppDataMutable(kAppId);
  app_data->SetStatusForTest(KioskAppData::Status::kError);

  // Copy test crx file to temp dir because the cache moves the file.
  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  base::FilePath data_dir =
      test_dir.AppendASCII("chromeos/app_mode/webstore/downloads/");
  base::FilePath crx_file = data_dir.AppendASCII(
      "pegeblegnlhnpgghhjblhchdllfijodp-2.0.0."
      "crx");
  crx_file = CopyFileToTempDir(crx_file);

  TestFuture<const std::string&, bool> result_future;
  manager()->PutValidatedExternalExtension(kAppId, crx_file, "2.0.0",
                                           result_future.GetCallback());
  ASSERT_TRUE(result_future.Get<1>());

  // Wait for 3 data loaded events at the most. One for crx putting into cache,
  // one for update check and one for crx unpack to fail which resets the app
  // data status into INIT stage.
  const size_t kMaxDataChange = 3;
  for (size_t i = 0;
       i < kMaxDataChange && app_data->status() != KioskAppData::Status::kInit;
       ++i) {
    waiter.Reset();
    waiter.Wait(1);
    EXPECT_EQ(waiter.data_change_count(), 1);
    EXPECT_EQ(waiter.data_load_failure_count(), 0);
  }
  ASSERT_EQ(KioskAppData::Status::kInit, app_data->status());
  CheckAppData(kAppId, kAppName, "");
}

IN_PROC_BROWSER_TEST_F(ChromeAppKioskAppManagerTest, BadApp) {
  AppDataLoadWaiter waiter(manager());
  AddConsumerKioskChromeAppForTesting(
      CHECK_DEREF(owner_settings_service_.get()), "unknown_app");
  waiter.Wait(2);
  EXPECT_EQ(waiter.data_change_count(), 1);
  EXPECT_EQ(waiter.data_load_failure_count(), 1);
  EXPECT_EQ("", GetAppIds());
}

IN_PROC_BROWSER_TEST_F(ChromeAppKioskAppManagerTest, GoodApp) {
  // Webstore data json is in
  //   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/detail/app_1
  const char kAppId[] = "app_1";
  fake_cws()->SetNoUpdate(kAppId);
  AppDataLoadWaiter waiter(manager());
  AddConsumerKioskChromeAppForTesting(
      CHECK_DEREF(owner_settings_service_.get()), kAppId);
  waiter.Wait(2);
  EXPECT_EQ(waiter.data_change_count(), 2);
  EXPECT_EQ(waiter.data_load_failure_count(), 0);

  CheckAppDataAndCache(kAppId, "Name of App 1", "");
}

IN_PROC_BROWSER_TEST_F(ChromeAppKioskAppManagerTest,
                       AppWithRequiredPlatformVersion) {
  // Webstore data json is in
  //   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/detail/
  //     app_with_required_platform_version
  const char kAppId[] = "app_with_required_platform_version";
  fake_cws()->SetNoUpdate(kAppId);
  AppDataLoadWaiter waiter(manager());
  AddConsumerKioskChromeAppForTesting(
      CHECK_DEREF(owner_settings_service_.get()), kAppId);
  waiter.Wait(2);
  EXPECT_EQ(waiter.data_change_count(), 2);
  EXPECT_EQ(waiter.data_load_failure_count(), 0);

  CheckAppDataAndCache(kAppId, "App with required platform version", "1234");
}

IN_PROC_BROWSER_TEST_F(ChromeAppKioskAppManagerTest,
                       AppWithBadRequiredPlatformVersion) {
  // Webstore data json is in
  //   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/detail/
  //     app_with_bad_required_platform_version
  const char kAppId[] = "app_with_bad_required_platform_version";
  fake_cws()->SetNoUpdate(kAppId);
  AppDataLoadWaiter waiter(manager());
  AddConsumerKioskChromeAppForTesting(
      CHECK_DEREF(owner_settings_service_.get()), kAppId);
  waiter.Wait(2);
  EXPECT_EQ(waiter.data_change_count(), 1);
  EXPECT_EQ(1, waiter.data_load_failure_count());

  EXPECT_EQ("", GetAppIds());
}

IN_PROC_BROWSER_TEST_F(ChromeAppKioskAppManagerTest, DownloadNewApp) {
  RunAddNewAppTest(kTestLocalFsKioskApp, "1.0.0", kTestLocalFsKioskAppName, "");
}

// Flaky https://crbug.com/1090937
IN_PROC_BROWSER_TEST_F(ChromeAppKioskAppManagerTest, RemoveApp) {
  // Add a new app.
  RunAddNewAppTest(kTestLocalFsKioskApp, "1.0.0", kTestLocalFsKioskAppName, "");
  ASSERT_EQ(1u, manager()->GetApps().size());
  base::FilePath crx_path;
  std::string version;
  EXPECT_TRUE(GetCachedCrx(kTestLocalFsKioskApp, &crx_path, &version));
  {
    base::ScopedAllowBlockingForTesting allow_io;
    EXPECT_TRUE(base::PathExists(crx_path));
  }
  EXPECT_EQ("1.0.0", version);

  // Remove the app now.
  RemoveConsumerKioskChromeAppForTesting(
      CHECK_DEREF(manager()), CHECK_DEREF(owner_settings_service_.get()),
      kTestLocalFsKioskApp);
  content::RunAllTasksUntilIdle();
  ASSERT_EQ(0u, manager()->GetApps().size());
  {
    base::ScopedAllowBlockingForTesting allow_io;
    EXPECT_FALSE(base::PathExists(crx_path));
  }
  EXPECT_FALSE(GetCachedCrx(kTestLocalFsKioskApp, &crx_path, &version));
}

IN_PROC_BROWSER_TEST_F(ChromeAppKioskAppManagerTest, UpdateApp) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-0848f806-ea30-48db-8025-da0fd707a5a2");

  base::HistogramTester histogram;

  // Add a version 1 app first.
  RunAddNewAppTest(kTestLocalFsKioskApp, "1.0.0", kTestLocalFsKioskAppName, "");
  ASSERT_EQ(1u, manager()->GetApps().size());
  base::FilePath crx_path;
  std::string version;
  EXPECT_TRUE(GetCachedCrx(kTestLocalFsKioskApp, &crx_path, &version));
  {
    base::ScopedAllowBlockingForTesting allow_io;
    EXPECT_TRUE(base::PathExists(crx_path));
  }
  EXPECT_EQ("1.0.0", version);

  // Update to version 2.
  fake_cws()->SetUpdateCrx(
      kTestLocalFsKioskApp,
      "abbjjkefakmllanciinhgjgjamdmlbdg_v2_read_and_verify_data.crx", "2.0.0");
  AppDataLoadWaiter waiter(manager());
  UpdateAppsFromPolicy();
  waiter.Wait(1);
  EXPECT_EQ(waiter.data_change_count(), 1);
  EXPECT_EQ(waiter.data_load_failure_count(), 0);

  // Verify the app has been updated to v2.
  ASSERT_EQ(1u, manager()->GetApps().size());
  base::FilePath new_crx_path;
  std::string new_version;
  EXPECT_TRUE(GetCachedCrx(kTestLocalFsKioskApp, &new_crx_path, &new_version));
  EXPECT_EQ("2.0.0", new_version);
  {
    base::ScopedAllowBlockingForTesting allow_io;
    EXPECT_TRUE(base::PathExists(new_crx_path));
  }
  // Get original version 2 source download crx file path.
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath v2_file_path = test_data_dir.Append(FILE_PATH_LITERAL(
      "chromeos/app_mode/webstore/downloads/"
      "abbjjkefakmllanciinhgjgjamdmlbdg_v2_read_and_verify_data.crx"));
  {
    base::ScopedAllowBlockingForTesting allow_io;
    EXPECT_TRUE(base::PathExists(v2_file_path));
  }
  EXPECT_TRUE(base::ContentsEqual(v2_file_path, new_crx_path));

  histogram.ExpectUniqueSample(
      kKioskPrimaryAppUpdateResultHistogram,
      KioskChromeAppManager::PrimaryAppDownloadResult::kSuccess,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(ChromeAppKioskAppManagerTest, UpdateAndRemoveApp) {
  // Add a version 1 app first.
  RunAddNewAppTest(kTestLocalFsKioskApp, "1.0.0", kTestLocalFsKioskAppName, "");
  ASSERT_EQ(1u, manager()->GetApps().size());
  base::FilePath v1_crx_path;
  std::string version;
  EXPECT_TRUE(GetCachedCrx(kTestLocalFsKioskApp, &v1_crx_path, &version));
  {
    base::ScopedAllowBlockingForTesting allow_io;
    EXPECT_TRUE(base::PathExists(v1_crx_path));
  }
  EXPECT_EQ("1.0.0", version);

  // Update to version 2.
  fake_cws()->SetUpdateCrx(
      kTestLocalFsKioskApp,
      "abbjjkefakmllanciinhgjgjamdmlbdg_v2_read_and_verify_data.crx", "2.0.0");
  AppDataLoadWaiter waiter(manager());
  UpdateAppsFromPolicy();
  waiter.Wait(1);
  EXPECT_EQ(waiter.data_change_count(), 1);
  EXPECT_EQ(waiter.data_load_failure_count(), 0);

  // Verify the app has been updated to v2.
  ASSERT_EQ(1u, manager()->GetApps().size());
  base::FilePath v2_crx_path;
  std::string new_version;
  EXPECT_TRUE(GetCachedCrx(kTestLocalFsKioskApp, &v2_crx_path, &new_version));
  EXPECT_EQ("2.0.0", new_version);
  // Verify both v1 and v2 crx files exist.
  {
    base::ScopedAllowBlockingForTesting allow_io;
    EXPECT_TRUE(base::PathExists(v1_crx_path));
    EXPECT_TRUE(base::PathExists(v2_crx_path));
  }

  // Remove the app now.
  RemoveConsumerKioskChromeAppForTesting(
      CHECK_DEREF(manager()), CHECK_DEREF(owner_settings_service_.get()),
      kTestLocalFsKioskApp);
  content::RunAllTasksUntilIdle();
  ASSERT_EQ(0u, manager()->GetApps().size());
  // Verify both v1 and v2 crx files are removed.
  {
    base::ScopedAllowBlockingForTesting allow_io;
    EXPECT_FALSE(base::PathExists(v1_crx_path));
    EXPECT_FALSE(base::PathExists(v2_crx_path));
  }
  EXPECT_FALSE(GetCachedCrx(kTestLocalFsKioskApp, &v2_crx_path, &version));
}

IN_PROC_BROWSER_TEST_F(ChromeAppKioskAppManagerTest,
                       GetAutoLaunchAppRequiredPlatformVersion) {
  const char kAppId[] = "app_with_required_platform_version";
  const char kRequiredPlatformVersion[] = "1234";
  SetExistingApp(kAppId, "App Name", "red16x16.png", kRequiredPlatformVersion);

  fake_cws()->SetNoUpdate(kAppId);
  AppDataLoadWaiter waiter(manager());
  waiter.Wait(1);
  EXPECT_EQ(waiter.data_change_count(), 1);
  EXPECT_EQ(waiter.data_load_failure_count(), 0);

  EXPECT_EQ("", manager()->GetAutoLaunchApp());
  EXPECT_EQ("", manager()->GetAutoLaunchAppRequiredPlatformVersion());

  SetConsumerKioskAutoLaunchChromeAppForTesting(
      CHECK_DEREF(manager()), CHECK_DEREF(owner_settings_service_.get()),
      kAppId);
  EXPECT_EQ(kAppId, manager()->GetAutoLaunchApp());
  EXPECT_EQ(kRequiredPlatformVersion,
            manager()->GetAutoLaunchAppRequiredPlatformVersion());

  // No require platform version if auto launched app has a non-zero delay set.
  settings_helper_.SetInteger(kAccountsPrefDeviceLocalAccountAutoLoginDelay, 1);
  EXPECT_EQ("", manager()->GetAutoLaunchAppRequiredPlatformVersion());

  settings_helper_.SetInteger(kAccountsPrefDeviceLocalAccountAutoLoginDelay, 0);
  EXPECT_EQ(kRequiredPlatformVersion,
            manager()->GetAutoLaunchAppRequiredPlatformVersion());
}

IN_PROC_BROWSER_TEST_F(ChromeAppKioskAppManagerTest, IsPlatformCompliant) {
  base::test::ScopedChromeOSVersionInfo version(
      "CHROMEOS_RELEASE_VERSION=1234.1.2", base::Time::Now());

  EXPECT_TRUE(manager()->IsPlatformCompliant(""));
  EXPECT_TRUE(manager()->IsPlatformCompliant("1234"));
  EXPECT_TRUE(manager()->IsPlatformCompliant("1234.1"));
  EXPECT_TRUE(manager()->IsPlatformCompliant("1234.1.2"));

  EXPECT_FALSE(manager()->IsPlatformCompliant("123"));
  EXPECT_FALSE(manager()->IsPlatformCompliant("1234.2"));
  EXPECT_FALSE(manager()->IsPlatformCompliant("1234.1.1"));
  EXPECT_FALSE(manager()->IsPlatformCompliant("1234.1.3"));

  EXPECT_FALSE(manager()->IsPlatformCompliant("1234.1.2.3"));
  EXPECT_FALSE(manager()->IsPlatformCompliant("bad version"));
}

IN_PROC_BROWSER_TEST_F(ChromeAppKioskAppManagerTest,
                       IsPlatformCompliantWithApp) {
  base::AddFeatureIdTagToTestResult(
      "screenplay-182c1317-2fc0-459b-b217-ec7d6de6e363");

  base::test::ScopedChromeOSVersionInfo version(
      "CHROMEOS_RELEASE_VERSION=1234.1.2", base::Time::Now());

  const char kAppId[] = "app_id";
  SetExistingApp(kAppId, "App Name", "red16x16.png", "");

  SetConsumerKioskAutoLaunchChromeAppForTesting(
      CHECK_DEREF(manager()), CHECK_DEREF(owner_settings_service_.get()),
      kAppId);
  manager()->SetAppWasAutoLaunchedWithZeroDelay(kAppId);

  struct {
    const std::string required_platform_version;
    const bool expected_compliant;
  } kTestCases[] = {
      {"", true},          {"1234", true},      {"1234.1", true},
      {"1234.1.2", true},  {"123", false},      {"1234.2", false},
      {"1234.1.1", false}, {"1234.1.3", false},
  };

  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    scoped_refptr<extensions::Extension> app = MakeKioskApp(
        "App Name", "1.0", kAppId, kTestCases[i].required_platform_version);
    EXPECT_EQ(kTestCases[i].expected_compliant,
              manager()->IsPlatformCompliantWithApp(app.get()))
        << "Test case: " << i << ", required_platform_version="
        << kTestCases[i].required_platform_version;
  }

  // If an app is not auto launched with zero delay, it is always compliant.
  const char kNoneAutoLaucnhedAppId[] = "none_auto_launch_app_id";
  for (size_t i = 0; i < std::size(kTestCases); ++i) {
    scoped_refptr<extensions::Extension> app =
        MakeKioskApp("App Name", "1.0", kNoneAutoLaucnhedAppId,
                     kTestCases[i].required_platform_version);
    EXPECT_TRUE(manager()->IsPlatformCompliantWithApp(app.get()))
        << "Test case for non auto launch app: " << i
        << ", required_platform_version="
        << kTestCases[i].required_platform_version;
  }
}

}  // namespace ash
