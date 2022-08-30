// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/plugin_vm_apps.h"

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class PluginVmAppsTest : public testing::Test {
 public:
  PluginVmAppsTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD) {}

  plugin_vm::PluginVmTestHelper* test_helper() { return test_helper_.get(); }

  AppServiceProxy* app_service_proxy() { return app_service_proxy_; }

  TestingProfile* profile() { return profile_.get(); }

  void SetUp() override {
    ash::CiceroneClient::InitializeFake();
    profile_ = std::make_unique<TestingProfile>();
    app_service_proxy_ = AppServiceProxyFactory::GetForProfile(profile_.get());
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile_.get());
    test_helper_ =
        std::make_unique<plugin_vm::PluginVmTestHelper>(profile_.get());
    test_helper_->AllowPluginVm();
  }

  void TearDown() override {
    test_helper_.reset();
    profile_.reset();
    ash::CiceroneClient::Shutdown();
  }

  // Set up the test Crostini app for our desired mime types.
  std::string AddPluginVmAppWithExtensionTypes(
      std::string app_id,
      std::string app_name,
      std::vector<std::string> extension_types) {
    vm_tools::apps::App app;
    for (std::string extension_type : extension_types) {
      app.add_extensions(extension_type);
    }
    app.set_desktop_file_id(app_id);
    vm_tools::apps::App::LocaleString::Entry* entry =
        app.mutable_name()->add_values();
    entry->set_locale(std::string());
    entry->set_value(app_name);
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
  AppServiceProxy* app_service_proxy_ = nullptr;
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<plugin_vm::PluginVmTestHelper> test_helper_;
};

TEST_F(PluginVmAppsTest, AppServiceHasPluginVmIntentFilters) {
  std::vector<std::string> extension_types = {"csv", "txt"};

  std::string app_id =
      AddPluginVmAppWithExtensionTypes("app_id", "app_name", extension_types);
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

}  // namespace apps
