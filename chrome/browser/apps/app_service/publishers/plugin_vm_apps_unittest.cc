// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/plugin_vm_apps.h"

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/escape.h"
#include "base/test/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_result_type.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/plugin_vm/mock_plugin_vm_manager.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_manager_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/dbus/chunneld/chunneld_client.h"
#include "chromeos/ash/components/dbus/cicerone/fake_cicerone_client.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/test/browser_task_environment.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_url.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

using MockPluginVmManager =
    testing::StrictMock<plugin_vm::test::MockPluginVmManager>;

class PluginVmAppsTest : public testing::Test {
 public:
  PluginVmAppsTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD) {}

  plugin_vm::PluginVmTestHelper* test_helper() { return test_helper_.get(); }

  AppServiceProxy* app_service_proxy() { return app_service_proxy_; }

  TestingProfile* profile() { return profile_.get(); }

  MockPluginVmManager* plugin_vm_manager() { return plugin_vm_manager_; }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    app_service_proxy_ = AppServiceProxyFactory::GetForProfile(profile_.get());
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile_.get());
    test_helper_ =
        std::make_unique<plugin_vm::PluginVmTestHelper>(profile_.get());
    test_helper_->AllowPluginVm();
    test_helper_->EnablePluginVm();
    mount_points_ = storage::ExternalMountPoints::GetSystemInstance();
    mount_name_ =
        file_manager::util::GetDownloadsMountPointName(profile_.get());
    mount_points_->RegisterFileSystem(
        mount_name_, storage::kFileSystemTypeLocal,
        storage::FileSystemMountOption(), GetMyFilesFolderPath());
    plugin_vm_manager_ = static_cast<MockPluginVmManager*>(
        plugin_vm::PluginVmManagerFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                profile_.get(),
                base::BindRepeating([](content::BrowserContext*)
                                        -> std::unique_ptr<KeyedService> {
                  return std::make_unique<MockPluginVmManager>();
                })));
  }

  void TearDown() override {
    base::DeletePathRecursively(GetPvmDefaultPath());
    mount_points_->RevokeAllFileSystems();
    test_helper_.reset();
    profile_.reset();
  }

  base::FilePath GetMyFilesFolderPath() {
    return file_manager::util::GetMyFilesFolderForProfile(profile_.get());
  }

  base::FilePath GetPvmDefaultPath() {
    return GetMyFilesFolderPath().Append("PvmDefault");
  }

  storage::FileSystemURL GetMyFilesFileSystemURL(const std::string& path) {
    return mount_points_->CreateExternalFileSystemURL(
        blink::StorageKey::CreateFirstParty(
            file_manager::util::GetFilesAppOrigin()),
        mount_name_, base::FilePath(path));
  }

  // Set up the test PluginVm app for our desired mime types.
  std::string AddPluginVmAppWithExtensionTypes(
      std::string app_id,
      std::vector<std::string> extension_types) {
    vm_tools::apps::App app;
    for (std::string extension_type : extension_types) {
      app.add_extensions(extension_type);
    }
    app.set_desktop_file_id(app_id);
    vm_tools::apps::App::LocaleString::Entry* entry =
        app.mutable_name()->add_values();
    entry->set_locale(std::string());
    entry->set_value(app_id);
    test_helper()->AddApp(app);
    return plugin_vm::PluginVmTestHelper::GenerateAppId(app.desktop_file_id());
  }

  // Get the registered intent filters for the app in App Service.
  std::vector<std::unique_ptr<IntentFilter>> GetIntentFiltersForApp(
      std::string app_id) {
    std::vector<std::unique_ptr<IntentFilter>> intent_filters;
    app_service_proxy()->AppRegistryCache().ForOneApp(
        app_id, [&intent_filters](const AppUpdate& update) {
          intent_filters = update.IntentFilters();
        });
    return intent_filters;
  }

 private:
  struct ScopedDBusClients {
    ScopedDBusClients() {
      ash::CiceroneClient::InitializeFake();
      ash::ConciergeClient::InitializeFake();
      ash::SeneschalClient::InitializeFake();
      ash::ChunneldClient::InitializeFake();
    }
    ~ScopedDBusClients() {
      ash::ChunneldClient::Shutdown();
      ash::SeneschalClient::Shutdown();
      ash::ConciergeClient::Shutdown();
      ash::CiceroneClient::Shutdown();
    }
  } dbus_clients_;

  raw_ptr<AppServiceProxy, DanglingUntriaged> app_service_proxy_ = nullptr;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<plugin_vm::PluginVmTestHelper> test_helper_;
  raw_ptr<storage::ExternalMountPoints> mount_points_;
  std::string mount_name_;
  raw_ptr<MockPluginVmManager, DanglingUntriaged> plugin_vm_manager_;
};

TEST_F(PluginVmAppsTest, AppServiceHasPluginVmIntentFilters) {
  std::vector<std::string> extension_types = {"csv", "txt"};

  std::string app_id =
      AddPluginVmAppWithExtensionTypes("app_id", extension_types);
  std::vector<std::unique_ptr<IntentFilter>> intent_filters =
      GetIntentFiltersForApp(app_id);

  EXPECT_EQ(intent_filters.size(), 1U);
  EXPECT_EQ(intent_filters[0]->conditions.size(), 2U);

  // Check that the filter has the correct action type.
  {
    const Condition* condition = intent_filters[0]->conditions[0].get();
    ASSERT_EQ(condition->condition_type, ConditionType::kAction);
    EXPECT_EQ(condition->condition_values.size(), 1U);
    ASSERT_EQ(condition->condition_values[0]->match_type,
              PatternMatchType::kLiteral);
    ASSERT_EQ(condition->condition_values[0]->value,
              apps_util::kIntentActionView);
  }

  // Check that the filter has all our extension types.
  {
    const Condition* condition = intent_filters[0]->conditions[1].get();
    ASSERT_EQ(condition->condition_type, ConditionType::kFile);
    EXPECT_EQ(condition->condition_values.size(), 2U);
    ASSERT_EQ(condition->condition_values[0]->value, extension_types[0]);
    ASSERT_EQ(condition->condition_values[1]->value, extension_types[1]);
  }
}

TEST_F(PluginVmAppsTest, LaunchAppWithIntent) {
  std::string app_id = AddPluginVmAppWithExtensionTypes("app_id", {"txt"});
  apps::IntentPtr intent =
      std::make_unique<apps::Intent>(apps_util::kIntentActionView);
  std::vector<apps::IntentFilePtr> files;

  // Add a file in the PvmDefault directory so that PluginVm has access to it.
  files.push_back(std::make_unique<apps::IntentFile>(
      GetMyFilesFileSystemURL("PvmDefault/file").ToGURL()));
  intent->files = {std::move(files)};

  // Retrieve the callback object when we reach the end of LaunchPluginVmApp().
  plugin_vm::PluginVmManager::LaunchPluginVmCallback launch_plugin_vm_callback;
  EXPECT_CALL(*plugin_vm_manager(), LaunchPluginVm(testing::_))
      .WillOnce(testing::Invoke(
          [&](plugin_vm::PluginVmManager::LaunchPluginVmCallback callback) {
            EXPECT_TRUE(launch_plugin_vm_callback.is_null());
            launch_plugin_vm_callback = std::move(callback);
          }));

  app_service_proxy()->LaunchAppWithIntent(
      app_id, /*event_flags=*/0, std::move(intent), LaunchSource::kUnknown,
      std::unique_ptr<WindowInfo>(), base::DoNothing());

  // Check that the callback is not null, i.e. that we reached the end of
  // LaunchPluginVmApp(). Getting to this point confirms that the arguments we
  // took from LaunchAppWithIntent were converted successfully for
  // LaunchPluginVmApp() to pass the validity checks inside LaunchPluginVmApp().
  ASSERT_FALSE(launch_plugin_vm_callback.is_null());
}

TEST_F(PluginVmAppsTest, LaunchAppWithIntent_FailedDirectoryNotShared) {
  std::string app_id = AddPluginVmAppWithExtensionTypes("app_id", {"txt"});
  apps::IntentPtr intent =
      std::make_unique<apps::Intent>(apps_util::kIntentActionView);
  std::vector<apps::IntentFilePtr> files;

  // Add a file in the downloads directory so that PluginVm does NOT have access
  // to it.
  files.push_back(std::make_unique<apps::IntentFile>(
      GetMyFilesFileSystemURL("Downloads/file").ToGURL()));
  intent->files = {std::move(files)};

  std::optional<State> result_state;
  app_service_proxy()->LaunchAppWithIntent(
      app_id, /*event_flags=*/0, std::move(intent), LaunchSource::kUnknown,
      std::unique_ptr<WindowInfo>(),
      base::BindLambdaForTesting(
          [&result_state](apps::LaunchResult&& callback_result) {
            result_state = callback_result.state;
          }));

  ASSERT_EQ(result_state.value_or(apps::State::kSuccess),
            apps::State::kFailedDirectoryNotShared);
}

}  // namespace apps
