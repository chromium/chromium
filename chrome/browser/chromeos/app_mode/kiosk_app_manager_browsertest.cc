// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"

#include <stddef.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/app_mode/fake_cws.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_data.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/chromeos/ownership/fake_owner_settings_service.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/policy/device_local_account.h"
#include "chrome/browser/chromeos/settings/scoped_cros_settings_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/chromeos_switches.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/test/test_utils.h"
#include "extensions/common/extension.h"
#include "net/base/host_port_pair.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

using content::BrowserThread;

namespace chromeos {

namespace {

// An app to test local fs data persistence across app update. V1 app writes
// data into local fs. V2 app reads and verifies the data.
// Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/bmbpicmpniaclbbpdkfglgipkkebnbjf
// The version 1.0.0 installed is in
//   chrome/test/data/chromeos/app_mode/webstore/downloads/
//       bmbpicmpniaclbbpdkfglgipkkebnbjf.crx
// The version 2.0.0 crx is in
//   chrome/test/data/chromeos/app_mode/webstore/downloads/
//       bmbpicmpniaclbbpdkfglgipkkebnbjf_v2_read_and_verify_data.crx
const char kTestLocalFsKioskApp[] = "bmbpicmpniaclbbpdkfglgipkkebnbjf";
const char kTestLocalFsKioskAppName[] = "Kiosk App With Local Data";

// Helper KioskAppManager::GetConsumerKioskAutoLaunchStatusCallback
// implementation.
void ConsumerKioskAutoLaunchStatusCheck(
    KioskAppManager::ConsumerKioskAutoLaunchStatus* out_status,
    const base::Closure& runner_quit_task,
    KioskAppManager::ConsumerKioskAutoLaunchStatus in_status) {
  LOG(INFO) << "ConsumerKioskAutoLaunchStatus = " << in_status;
  *out_status = in_status;
  runner_quit_task.Run();
}

// Helper KioskAppManager::EnableKioskModeCallback implementation.
void ConsumerKioskModeLockCheck(
    bool* out_locked,
    const base::Closure& runner_quit_task,
    bool in_locked) {
  LOG(INFO) << "kiosk locked  = " << in_locked;
  *out_locked = in_locked;
  runner_quit_task.Run();
}

// Helper InstallAttributes::LockResultCallback implementation.
void OnEnterpriseDeviceLock(InstallAttributes::LockResult* out_locked,
                            const base::Closure& runner_quit_task,
                            InstallAttributes::LockResult in_locked) {
  LOG(INFO) << "Enterprise lock  = " << in_locked;
  *out_locked = in_locked;
  runner_quit_task.Run();
}

scoped_refptr<extensions::Extension> MakeKioskApp(
    const std::string& name,
    const std::string& version,
    const std::string& id,
    const std::string& required_platform_version) {
  base::DictionaryValue value;
  value.SetString("name", name);
  value.SetString("version", version);
  std::unique_ptr<base::ListValue> scripts(new base::ListValue);
  scripts->AppendString("main.js");
  value.Set("app.background.scripts", std::move(scripts));
  value.SetBoolean("kiosk_enabled", true);
  if (!required_platform_version.empty()) {
    value.SetString("kiosk.required_platform_version",
                    required_platform_version);
  }

  std::string err;
  scoped_refptr<extensions::Extension> app =
      extensions::Extension::Create(
          base::FilePath(),
          extensions::Manifest::INTERNAL,
          value,
          extensions::Extension::WAS_INSTALLED_BY_DEFAULT,
          id,
          &err);
  EXPECT_EQ(err, "");
  return app;
}

void SetPlatformVersion(const std::string& platform_version) {
  const std::string lsb_release = base::StringPrintf(
      "CHROMEOS_RELEASE_VERSION=%s", platform_version.c_str());
  base::SysInfo::SetChromeOSVersionInfoForTest(lsb_release, base::Time::Now());
}

class AppDataLoadWaiter : public KioskAppManagerObserver {
 public:
  AppDataLoadWaiter(KioskAppManager* manager, int expected_data_change)
      : manager_(manager), expected_data_change_(expected_data_change) {
    manager_->AddObserver(this);
  }

  ~AppDataLoadWaiter() override { manager_->RemoveObserver(this); }

  void Wait() {
    if (quit_)
      return;
    runner_ = new content::MessageLoopRunner;
    runner_->Run();
  }

  void Reset() {
    quit_ = false;
    data_change_count_ = 0;
    data_load_failure_count_ = 0;
  }

  bool loaded() const { return loaded_; }
  int data_load_failure_count() const { return data_load_failure_count_; }

 private:
  // KioskAppManagerObserver overrides:
  void OnKioskAppDataChanged(const std::string& app_id) override {
    ++data_change_count_;
    if (data_change_count_ < expected_data_change_)
      return;
    loaded_ = true;
    quit_ = true;
    if (runner_.get())
      runner_->Quit();
  }

  void OnKioskAppDataLoadFailure(const std::string& app_id) override {
    ++data_load_failure_count_;
    loaded_ = false;
    quit_ = true;
    if (runner_.get())
      runner_->Quit();
  }

  void OnKioskExtensionLoadedInCache(const std::string& app_id) override {
    OnKioskAppDataChanged(app_id);
  }

  void OnKioskExtensionDownloadFailed(const std::string& app_id) override {
    OnKioskAppDataLoadFailure(app_id);
  }

  scoped_refptr<content::MessageLoopRunner> runner_;
  KioskAppManager* manager_;
  bool loaded_ = false;
  bool quit_ = false;
  int data_change_count_ = 0;
  int expected_data_change_;
  int data_load_failure_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(AppDataLoadWaiter);
};

// A class to wait for ExternalCache to finish putting the extension crx.
class ExternalCachePutWaiter {
 public:
  ExternalCachePutWaiter() {}
  ~ExternalCachePutWaiter() {}

  void Wait() {
    if (quit_)
      return;
    runner_ = new content::MessageLoopRunner;
    runner_->Run();
  }

  void OnPutExtension(const std::string& id, bool success) {
    success_ = success;
    quit_ = true;
    if (runner_.get())
      runner_->Quit();
  }

  bool success() const { return success_; }

 private:
  scoped_refptr<content::MessageLoopRunner> runner_;
  bool quit_ = false;
  bool success_ = false;

  DISALLOW_COPY_AND_ASSIGN(ExternalCachePutWaiter);
};

}  // namespace

class KioskAppManagerTest : public InProcessBrowserTest {
 public:
  KioskAppManagerTest() : settings_helper_(false), fake_cws_(new FakeCWS()) {}
  ~KioskAppManagerTest() override {}

  // InProcessBrowserTest overrides:
  void SetUp() override {
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);

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
    settings_helper_.RestoreRealDeviceSettingsProvider();
  }

  std::string GetAppIds() const {
    KioskAppManager::Apps apps;
    manager()->GetApps(&apps);

    std::string str;
    for (size_t i = 0; i < apps.size(); ++i) {
      if (i > 0)
        str += ',';
      str += apps[i].app_id;
    }

    return str;
  }

  // Locks device for enterprise.
  InstallAttributes::LockResult LockDeviceForEnterprise() {
    std::unique_ptr<InstallAttributes::LockResult> lock_result =
        std::make_unique<InstallAttributes::LockResult>(
            InstallAttributes::LOCK_NOT_READY);
    scoped_refptr<content::MessageLoopRunner> runner =
        new content::MessageLoopRunner;
    policy::BrowserPolicyConnectorChromeOS* connector =
        g_browser_process->platform_part()->browser_policy_connector_chromeos();
    connector->GetInstallAttributes()->LockDevice(
        policy::DEVICE_MODE_ENTERPRISE,
        "domain.com",
        std::string(),  // realm
        "device-id",
        base::Bind(
            &OnEnterpriseDeviceLock, lock_result.get(), runner->QuitClosure()));
    runner->Run();
    return *lock_result.get();
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

    std::unique_ptr<base::DictionaryValue> apps_dict(new base::DictionaryValue);
    apps_dict->SetString(app_id + ".name", app_name);
    apps_dict->SetString(app_id + ".icon", icon_path.MaybeAsASCII());
    apps_dict->SetString(app_id + ".required_platform_version",
                         required_platform_version);

    PrefService* local_state = g_browser_process->local_state();
    DictionaryPrefUpdate dict_update(local_state,
                                     KioskAppManager::kKioskDictionaryName);
    dict_update->Set(KioskAppDataBase::kKeyApps, std::move(apps_dict));

    // Make the app appear in device settings.
    base::ListValue device_local_accounts;
    std::unique_ptr<base::DictionaryValue> entry(new base::DictionaryValue);
    // Fake an account id. Note this needs to match GenerateKioskAppAccountId
    // in kiosk_app_manager.cc to make SetAutoLaunchApp work with the
    // existing app entry created here.
    entry->SetKey(kAccountsPrefDeviceLocalAccountsKeyId,
                  base::Value(app_id + "@kiosk-apps"));
    entry->SetKey(kAccountsPrefDeviceLocalAccountsKeyType,
                  base::Value(policy::DeviceLocalAccount::TYPE_KIOSK_APP));
    entry->SetKey(kAccountsPrefDeviceLocalAccountsKeyKioskAppId,
                  base::Value(app_id));
    device_local_accounts.Append(std::move(entry));
    owner_settings_service_->Set(kAccountsPrefDeviceLocalAccounts,
                                 device_local_accounts);
  }

  bool GetCachedCrx(const std::string& app_id,
                    base::FilePath* file_path,
                    std::string* version) {
    return manager()->GetCachedCrx(app_id, file_path, version);
  }

  void UpdateAppData() { manager()->UpdateAppData(); }

  void CheckAppData(const std::string& app_id,
                    const std::string& expected_app_name,
                    const std::string& expected_required_platform_version) {
    // Check manifest data is cached correctly.
    KioskAppManager::Apps apps;
    manager()->GetApps(&apps);
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
    const base::DictionaryValue* dict =
        local_state->GetDictionary(KioskAppManager::kKioskDictionaryName);

    std::string name;
    const std::string name_key = "apps." + app_id + ".name";
    EXPECT_TRUE(dict->GetString(name_key, &name));
    EXPECT_EQ(expected_app_name, name);

    std::string icon_path_string;
    const std::string icon_path_key = "apps." + app_id + ".icon";
    EXPECT_TRUE(dict->GetString(icon_path_key, &icon_path_string));

    std::string required_platform_version;
    const std::string required_platform_version_key =
        "apps." + app_id + ".required_platform_version";
    EXPECT_TRUE(dict->GetString(required_platform_version_key,
                                &required_platform_version));
    EXPECT_EQ(expected_required_platform_version, required_platform_version);

    base::FilePath expected_icon_path;
    ASSERT_TRUE(
        base::PathService::Get(chrome::DIR_USER_DATA, &expected_icon_path));
    expected_icon_path =
        expected_icon_path.AppendASCII(KioskAppManager::kIconCacheDir)
            .AppendASCII(app_id)
            .AddExtension(".png");
    EXPECT_EQ(expected_icon_path.value(), icon_path_string);
  }

  void RunAddNewAppTest(const std::string& id,
                        const std::string& expected_version,
                        const std::string& expected_app_name,
                        const std::string& expected_required_platform_version) {
    std::string crx_file_name = id + ".crx";
    fake_cws_->SetUpdateCrx(id, crx_file_name, expected_version);

    AppDataLoadWaiter waiter(manager(), 3);
    manager()->AddApp(id, owner_settings_service_.get());
    waiter.Wait();
    EXPECT_TRUE(waiter.loaded());

    // Check CRX file is cached.
    base::FilePath crx_path;
    std::string crx_version;
    EXPECT_TRUE(GetCachedCrx(id, &crx_path, &crx_version));
    EXPECT_TRUE(base::PathExists(crx_path));
    EXPECT_EQ(expected_version, crx_version);
    // Verify the original crx file is identical to the cached file.
    base::FilePath test_data_dir;
    base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
    std::string src_file_path_str =
        std::string("chromeos/app_mode/webstore/downloads/") + crx_file_name;
    base::FilePath src_file_path = test_data_dir.Append(src_file_path_str);
    EXPECT_TRUE(base::PathExists(src_file_path));
    EXPECT_TRUE(base::ContentsEqual(src_file_path, crx_path));

    CheckAppDataAndCache(id, expected_app_name,
                         expected_required_platform_version);
  }

  // Copies the given file into temp dir and returns the full path
  // of the copied file.
  base::FilePath CopyFileToTempDir(const base::FilePath& file) {
    base::FilePath target_file = temp_dir_.GetPath().Append(file.BaseName());
    CHECK(base::CopyFile(file, target_file));
    return target_file;
  }

  KioskAppData* GetAppDataMutable(const std::string& app_id) {
    return manager()->GetAppDataMutable(app_id);
  }

  KioskAppManager* manager() const { return KioskAppManager::Get(); }
  FakeCWS* fake_cws() { return fake_cws_.get(); }

 protected:
  ScopedCrosSettingsTestHelper settings_helper_;
  std::unique_ptr<FakeOwnerSettingsService> owner_settings_service_;

 private:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<FakeCWS> fake_cws_;

  DISALLOW_COPY_AND_ASSIGN(KioskAppManagerTest);
};

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, Basic) {
  // Add a couple of apps. Use "fake_app_x" that do not have data on the test
  // server to avoid pending data loads that could be lingering on tear down and
  // cause DCHECK failure in utility_process_host.cc.
  manager()->AddApp("fake_app_1", owner_settings_service_.get());
  manager()->AddApp("fake_app_2", owner_settings_service_.get());
  EXPECT_EQ("fake_app_1,fake_app_2", GetAppIds());

  // Set an auto launch app.
  manager()->SetAutoLaunchApp("fake_app_1", owner_settings_service_.get());
  EXPECT_EQ("fake_app_1", manager()->GetAutoLaunchApp());

  // Make sure that if an app was auto launched with zero delay, it is reflected
  // in the app data.
  KioskAppManager::App app;
  manager()->GetApp("fake_app_1", &app);
  EXPECT_FALSE(app.was_auto_launched_with_zero_delay);

  manager()->SetAppWasAutoLaunchedWithZeroDelay("fake_app_1");
  manager()->GetApp("fake_app_1", &app);
  EXPECT_TRUE(app.was_auto_launched_with_zero_delay);

  // Clear the auto launch app.
  manager()->SetAutoLaunchApp("", owner_settings_service_.get());
  EXPECT_EQ("", manager()->GetAutoLaunchApp());
  EXPECT_FALSE(manager()->IsAutoLaunchEnabled());

  // App should still report it was auto launched with zero delay, even though
  // it is no longer set to auto launch in the future.
  manager()->GetApp("fake_app_1", &app);
  EXPECT_TRUE(app.was_auto_launched_with_zero_delay);

  // Set another auto launch app.
  manager()->SetAutoLaunchApp("fake_app_2", owner_settings_service_.get());
  EXPECT_EQ("fake_app_2", manager()->GetAutoLaunchApp());

  // Check auto launch permissions.
  EXPECT_FALSE(manager()->IsAutoLaunchEnabled());
  manager()->SetEnableAutoLaunch(true);
  EXPECT_TRUE(manager()->IsAutoLaunchEnabled());

  // Remove the auto launch app.
  manager()->RemoveApp("fake_app_2", owner_settings_service_.get());
  EXPECT_EQ("fake_app_1", GetAppIds());
  EXPECT_EQ("", manager()->GetAutoLaunchApp());

  // Add the just removed auto launch app again and it should no longer be
  // the auto launch app.
  manager()->AddApp("fake_app_2", owner_settings_service_.get());
  EXPECT_EQ("", manager()->GetAutoLaunchApp());
  manager()->RemoveApp("fake_app_2", owner_settings_service_.get());
  EXPECT_EQ("fake_app_1", GetAppIds());

  // Set a none exist app as auto launch.
  manager()->SetAutoLaunchApp("none_exist_app", owner_settings_service_.get());
  EXPECT_EQ("", manager()->GetAutoLaunchApp());
  EXPECT_FALSE(manager()->IsAutoLaunchEnabled());

  // Add an existing app again.
  manager()->AddApp("fake_app_1", owner_settings_service_.get());
  EXPECT_EQ("fake_app_1", GetAppIds());
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, LoadCached) {
  SetExistingApp("app_1", "Cached App1 Name", "red16x16.png", "1234");

  fake_cws()->SetNoUpdate("app_1");
  AppDataLoadWaiter waiter(manager(), 1);
  waiter.Wait();
  EXPECT_TRUE(waiter.loaded());

  CheckAppData("app_1", "Cached App1 Name", "1234");
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, ClearAppData) {
  SetExistingApp("app_1", "Cached App1 Name", "red16x16.png", "");

  PrefService* local_state = g_browser_process->local_state();
  const base::DictionaryValue* dict =
      local_state->GetDictionary(KioskAppManager::kKioskDictionaryName);
  const base::DictionaryValue* apps_dict;
  EXPECT_TRUE(dict->GetDictionary(KioskAppDataBase::kKeyApps, &apps_dict));
  EXPECT_TRUE(apps_dict->HasKey("app_1"));

  manager()->ClearAppData("app_1");

  EXPECT_FALSE(apps_dict->HasKey("app_1"));
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, UpdateAppDataFromProfile) {
  SetExistingApp("app_1", "Cached App1 Name", "red16x16.png", "");

  fake_cws()->SetNoUpdate("app_1");
  AppDataLoadWaiter waiter(manager(), 1);
  waiter.Wait();
  EXPECT_TRUE(waiter.loaded());

  CheckAppData("app_1", "Cached App1 Name", "");

  scoped_refptr<extensions::Extension> updated_app =
      MakeKioskApp("Updated App1 Name", "2.0", "app_1", "1234");
  manager()->UpdateAppDataFromProfile(
      "app_1", browser()->profile(), updated_app.get());

  waiter.Reset();
  waiter.Wait();
  EXPECT_TRUE(waiter.loaded());

  CheckAppData("app_1", "Updated App1 Name", "1234");
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, UpdateAppDataFromCrx) {
  const char kAppId[] = "ajoggoflpgplnnjkjamcmbepjdjdnpdp";
  const char kAppName[] = "Test Kiosk App";

  SetExistingApp(kAppId, kAppName, "red16x16.png", "");
  fake_cws()->SetNoUpdate(kAppId);
  AppDataLoadWaiter waiter(manager(), 1);
  waiter.Wait();
  EXPECT_TRUE(waiter.loaded());

  CheckAppData(kAppId, kAppName, "");

  // Fake app data load failure so that the manager will attempt to
  // load it from crx.
  KioskAppData* app_data = GetAppDataMutable(kAppId);
  app_data->SetStatusForTest(KioskAppData::STATUS_ERROR);

  // Copy test crx file to temp dir because the cache moves the file.
  base::FilePath test_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
  base::FilePath data_dir =
      test_dir.AppendASCII("chromeos/app_mode/webstore/downloads/");
  base::FilePath crx_file = data_dir.AppendASCII(
      "ajoggoflpgplnnjkjamcmbepjdjdnpdp_v2_required_platform_version_added."
      "crx");
  crx_file = CopyFileToTempDir(crx_file);

  ExternalCachePutWaiter put_waiter;
  manager()->PutValidatedExternalExtension(
      kAppId, crx_file, "2.0.0",
      base::BindOnce(&ExternalCachePutWaiter::OnPutExtension,
                     base::Unretained(&put_waiter)));
  put_waiter.Wait();
  ASSERT_TRUE(put_waiter.success());

  // Wait for 3 data loaded events at the most. One for crx putting into cache,
  // one for update check and one for app data is updated from crx.
  const size_t kMaxDataChange = 3;
  for (size_t i = 0;
       i < kMaxDataChange && app_data->status() != KioskAppData::STATUS_LOADED;
       ++i) {
    waiter.Reset();
    waiter.Wait();
  }
  ASSERT_EQ(KioskAppData::STATUS_LOADED, app_data->status());

  CheckAppData(kAppId, kAppName, "1234");
}

// Flaky: https://crbug.com/837195
IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, DISABLED_BadApp) {
  AppDataLoadWaiter waiter(manager(), 2);
  manager()->AddApp("unknown_app", owner_settings_service_.get());
  waiter.Wait();
  EXPECT_FALSE(waiter.loaded());
  EXPECT_EQ("", GetAppIds());
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, GoodApp) {
  // Webstore data json is in
  //   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/detail/app_1
  const char kAppId[] = "app_1";
  fake_cws()->SetNoUpdate(kAppId);
  AppDataLoadWaiter waiter(manager(), 2);
  manager()->AddApp(kAppId, owner_settings_service_.get());
  waiter.Wait();
  EXPECT_TRUE(waiter.loaded());

  CheckAppDataAndCache(kAppId, "Name of App 1", "");
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, AppWithRequiredPlatformVersion) {
  // Webstore data json is in
  //   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/detail/
  //     app_with_required_platform_version
  const char kAppId[] = "app_with_required_platform_version";
  fake_cws()->SetNoUpdate(kAppId);
  AppDataLoadWaiter waiter(manager(), 2);
  manager()->AddApp(kAppId, owner_settings_service_.get());
  waiter.Wait();
  EXPECT_TRUE(waiter.loaded());

  CheckAppDataAndCache(kAppId, "App with required platform version", "1234");
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, AppWithBadRequiredPlatformVersion) {
  // Webstore data json is in
  //   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/detail/
  //     app_with_bad_required_platform_version
  const char kAppId[] = "app_with_bad_required_platform_version";
  fake_cws()->SetNoUpdate(kAppId);
  AppDataLoadWaiter waiter(manager(), 2);
  manager()->AddApp(kAppId, owner_settings_service_.get());
  waiter.Wait();
  EXPECT_FALSE(waiter.loaded());
  EXPECT_EQ(1, waiter.data_load_failure_count());

  EXPECT_EQ("", GetAppIds());
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, DownloadNewApp) {
  RunAddNewAppTest(kTestLocalFsKioskApp, "1.0.0", kTestLocalFsKioskAppName, "");
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, RemoveApp) {
  // Add a new app.
  RunAddNewAppTest(kTestLocalFsKioskApp, "1.0.0", kTestLocalFsKioskAppName, "");
  KioskAppManager::Apps apps;
  manager()->GetApps(&apps);
  ASSERT_EQ(1u, apps.size());
  base::FilePath crx_path;
  std::string version;
  EXPECT_TRUE(GetCachedCrx(kTestLocalFsKioskApp, &crx_path, &version));
  EXPECT_TRUE(base::PathExists(crx_path));
  EXPECT_EQ("1.0.0", version);

  // Remove the app now.
  manager()->RemoveApp(kTestLocalFsKioskApp, owner_settings_service_.get());
  content::RunAllTasksUntilIdle();
  manager()->GetApps(&apps);
  ASSERT_EQ(0u, apps.size());
  EXPECT_FALSE(base::PathExists(crx_path));
  EXPECT_FALSE(GetCachedCrx(kTestLocalFsKioskApp, &crx_path, &version));
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, UpdateApp) {
  // Add a version 1 app first.
  RunAddNewAppTest(kTestLocalFsKioskApp, "1.0.0", kTestLocalFsKioskAppName, "");
  KioskAppManager::Apps apps;
  manager()->GetApps(&apps);
  ASSERT_EQ(1u, apps.size());
  base::FilePath crx_path;
  std::string version;
  EXPECT_TRUE(GetCachedCrx(kTestLocalFsKioskApp, &crx_path, &version));
  EXPECT_TRUE(base::PathExists(crx_path));
  EXPECT_EQ("1.0.0", version);

  // Update to version 2.
  fake_cws()->SetUpdateCrx(
      kTestLocalFsKioskApp,
      "bmbpicmpniaclbbpdkfglgipkkebnbjf_v2_read_and_verify_data.crx",
      "2.0.0");
  AppDataLoadWaiter waiter(manager(), 1);
  UpdateAppData();
  waiter.Wait();
  EXPECT_TRUE(waiter.loaded());

  // Verify the app has been updated to v2.
  manager()->GetApps(&apps);
  ASSERT_EQ(1u, apps.size());
  base::FilePath new_crx_path;
  std::string new_version;
  EXPECT_TRUE(GetCachedCrx(kTestLocalFsKioskApp, &new_crx_path, &new_version));
  EXPECT_EQ("2.0.0", new_version);
  EXPECT_TRUE(base::PathExists(new_crx_path));
  // Get original version 2 source download crx file path.
  base::FilePath test_data_dir;
  base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir);
  base::FilePath v2_file_path = test_data_dir.Append(FILE_PATH_LITERAL(
      "chromeos/app_mode/webstore/downloads/"
      "bmbpicmpniaclbbpdkfglgipkkebnbjf_v2_read_and_verify_data.crx"));
  EXPECT_TRUE(base::PathExists(v2_file_path));
  EXPECT_TRUE(base::ContentsEqual(v2_file_path, new_crx_path));
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, UpdateAndRemoveApp) {
  // Add a version 1 app first.
  RunAddNewAppTest(kTestLocalFsKioskApp, "1.0.0", kTestLocalFsKioskAppName, "");
  KioskAppManager::Apps apps;
  manager()->GetApps(&apps);
  ASSERT_EQ(1u, apps.size());
  base::FilePath v1_crx_path;
  std::string version;
  EXPECT_TRUE(GetCachedCrx(kTestLocalFsKioskApp, &v1_crx_path, &version));
  EXPECT_TRUE(base::PathExists(v1_crx_path));
  EXPECT_EQ("1.0.0", version);

  // Update to version 2.
  fake_cws()->SetUpdateCrx(
      kTestLocalFsKioskApp,
      "bmbpicmpniaclbbpdkfglgipkkebnbjf_v2_read_and_verify_data.crx",
      "2.0.0");
  AppDataLoadWaiter waiter(manager(), 1);
  UpdateAppData();
  waiter.Wait();
  EXPECT_TRUE(waiter.loaded());

  // Verify the app has been updated to v2.
  manager()->GetApps(&apps);
  ASSERT_EQ(1u, apps.size());
  base::FilePath v2_crx_path;
  std::string new_version;
  EXPECT_TRUE(GetCachedCrx(kTestLocalFsKioskApp, &v2_crx_path, &new_version));
  EXPECT_EQ("2.0.0", new_version);
  // Verify both v1 and v2 crx files exist.
  EXPECT_TRUE(base::PathExists(v1_crx_path));
  EXPECT_TRUE(base::PathExists(v2_crx_path));

  // Remove the app now.
  manager()->RemoveApp(kTestLocalFsKioskApp, owner_settings_service_.get());
  content::RunAllTasksUntilIdle();
  manager()->GetApps(&apps);
  ASSERT_EQ(0u, apps.size());
  // Verify both v1 and v2 crx files are removed.
  EXPECT_FALSE(base::PathExists(v1_crx_path));
  EXPECT_FALSE(base::PathExists(v2_crx_path));
  EXPECT_FALSE(GetCachedCrx(kTestLocalFsKioskApp, &v2_crx_path, &version));
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, EnableConsumerKiosk) {
  // Consumer kiosk is disabled by default. Enable it for test.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableConsumerKiosk);

  KioskAppManager::ConsumerKioskAutoLaunchStatus status =
      KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_DISABLED;
  bool locked = false;

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  manager()->GetConsumerKioskAutoLaunchStatus(base::Bind(
      &ConsumerKioskAutoLaunchStatusCheck, &status, runner->QuitClosure()));
  runner->Run();
  EXPECT_EQ(status, KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE);

  scoped_refptr<content::MessageLoopRunner> runner2 =
      new content::MessageLoopRunner;
  manager()->EnableConsumerKioskAutoLaunch(
      base::Bind(&ConsumerKioskModeLockCheck, &locked, runner2->QuitClosure()));
  runner2->Run();
  EXPECT_TRUE(locked);

  scoped_refptr<content::MessageLoopRunner> runner3 =
      new content::MessageLoopRunner;
  manager()->GetConsumerKioskAutoLaunchStatus(base::Bind(
      &ConsumerKioskAutoLaunchStatusCheck, &status, runner3->QuitClosure()));
  runner3->Run();
  EXPECT_EQ(status, KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_ENABLED);
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, ConsumerKioskDisabled) {
  KioskAppManager::ConsumerKioskAutoLaunchStatus status =
      KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_CONFIGURABLE;

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  manager()->GetConsumerKioskAutoLaunchStatus(base::Bind(
      &ConsumerKioskAutoLaunchStatusCheck, &status, runner->QuitClosure()));
  runner->Run();
  EXPECT_EQ(status, KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_DISABLED);
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest,
                       PreventEnableConsumerKioskForEnterprise) {
  // Consumer kiosk is disabled by default. Enable it for test.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableConsumerKiosk);

  // Lock the device as enterprise.
  EXPECT_EQ(LockDeviceForEnterprise(), InstallAttributes::LOCK_SUCCESS);

  KioskAppManager::ConsumerKioskAutoLaunchStatus status =
      KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_DISABLED;
  bool locked = true;

  scoped_refptr<content::MessageLoopRunner> runner =
      new content::MessageLoopRunner;
  manager()->GetConsumerKioskAutoLaunchStatus(base::Bind(
      &ConsumerKioskAutoLaunchStatusCheck, &status, runner->QuitClosure()));
  runner->Run();
  EXPECT_EQ(status, KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_DISABLED);

  scoped_refptr<content::MessageLoopRunner> runner2 =
      new content::MessageLoopRunner;
  manager()->EnableConsumerKioskAutoLaunch(
      base::Bind(&ConsumerKioskModeLockCheck, &locked, runner2->QuitClosure()));
  runner2->Run();
  EXPECT_FALSE(locked);

  scoped_refptr<content::MessageLoopRunner> runner3 =
      new content::MessageLoopRunner;
  manager()->GetConsumerKioskAutoLaunchStatus(base::Bind(
      &ConsumerKioskAutoLaunchStatusCheck, &status, runner3->QuitClosure()));
  runner3->Run();
  EXPECT_EQ(status, KioskAppManager::CONSUMER_KIOSK_AUTO_LAUNCH_DISABLED);
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest,
                       GetAutoLaunchAppRequiredPlatformVersion) {
  const char kAppId[] = "app_with_required_platform_version";
  const char kRequiredPlatformVersion[] = "1234";
  SetExistingApp(kAppId, "App Name", "red16x16.png", kRequiredPlatformVersion);

  fake_cws()->SetNoUpdate(kAppId);
  AppDataLoadWaiter waiter(manager(), 1);
  waiter.Wait();
  EXPECT_TRUE(waiter.loaded());

  EXPECT_FALSE(manager()->IsAutoLaunchEnabled());
  EXPECT_EQ("", manager()->GetAutoLaunchAppRequiredPlatformVersion());

  manager()->SetAutoLaunchApp(kAppId, owner_settings_service_.get());
  EXPECT_EQ("", manager()->GetAutoLaunchAppRequiredPlatformVersion());

  manager()->SetEnableAutoLaunch(true);
  EXPECT_TRUE(manager()->IsAutoLaunchEnabled());
  EXPECT_EQ(kRequiredPlatformVersion,
            manager()->GetAutoLaunchAppRequiredPlatformVersion());

  // No require platform version if auto launched app has a non-zero delay set.
  settings_helper_.SetInteger(kAccountsPrefDeviceLocalAccountAutoLoginDelay, 1);
  EXPECT_EQ("", manager()->GetAutoLaunchAppRequiredPlatformVersion());

  settings_helper_.SetInteger(kAccountsPrefDeviceLocalAccountAutoLoginDelay, 0);
  EXPECT_EQ(kRequiredPlatformVersion,
            manager()->GetAutoLaunchAppRequiredPlatformVersion());
}

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, IsPlatformCompliant) {
  SetPlatformVersion("1234.1.2");

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

IN_PROC_BROWSER_TEST_F(KioskAppManagerTest, IsPlatformCompliantWithApp) {
  SetPlatformVersion("1234.1.2");

  const char kAppId[] = "app_id";
  SetExistingApp(kAppId, "App Name", "red16x16.png", "");

  manager()->SetAutoLaunchApp(kAppId, owner_settings_service_.get());
  manager()->SetEnableAutoLaunch(true);
  manager()->SetAppWasAutoLaunchedWithZeroDelay(kAppId);

  struct {
    const std::string required_platform_version;
    const bool expected_compliant;
  } kTestCases[] = {
      {"", true},          {"1234", true},      {"1234.1", true},
      {"1234.1.2", true},  {"123", false},      {"1234.2", false},
      {"1234.1.1", false}, {"1234.1.3", false},
  };

  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    scoped_refptr<extensions::Extension> app = MakeKioskApp(
        "App Name", "1.0", kAppId, kTestCases[i].required_platform_version);
    EXPECT_EQ(kTestCases[i].expected_compliant,
              manager()->IsPlatformCompliantWithApp(app.get()))
        << "Test case: " << i << ", required_platform_version="
        << kTestCases[i].required_platform_version;
  }

  // If an app is not auto launched with zero delay, it is always compliant.
  const char kNoneAutoLaucnhedAppId[] = "none_auto_launch_app_id";
  for (size_t i = 0; i < arraysize(kTestCases); ++i) {
    scoped_refptr<extensions::Extension> app =
        MakeKioskApp("App Name", "1.0", kNoneAutoLaucnhedAppId,
                     kTestCases[i].required_platform_version);
    EXPECT_TRUE(manager()->IsPlatformCompliantWithApp(app.get()))
        << "Test case for non auto launch app: " << i
        << ", required_platform_version="
        << kTestCases[i].required_platform_version;
  }
}

}  // namespace chromeos
