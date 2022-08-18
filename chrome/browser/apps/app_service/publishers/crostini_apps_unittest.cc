// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/crostini_apps.h"

#include <vector>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/cicerone/cicerone_client.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

class CrostiniAppsTest : public testing::Test {
 public:
  CrostiniAppsTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD) {}

  crostini::CrostiniTestHelper* test_helper() { return test_helper_.get(); }

  AppServiceProxy* app_service_proxy() { return app_service_proxy_; }

  TestingProfile* profile() { return profile_.get(); }

  void SetUp() override {
    ash::CiceroneClient::InitializeFake();
    profile_ = std::make_unique<TestingProfile>();
    app_service_proxy_ = AppServiceProxyFactory::GetForProfile(profile_.get());
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile_.get());
    test_helper_ =
        std::make_unique<crostini::CrostiniTestHelper>(profile_.get());
    test_helper()->ReInitializeAppServiceIntegration();
  }

  void TearDown() override {
    test_helper_.reset();
    profile_.reset();
    ash::CiceroneClient::Shutdown();
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<crostini::CrostiniTestHelper> test_helper_;
  AppServiceProxy* app_service_proxy_ = nullptr;
};

TEST_F(CrostiniAppsTest, AppServiceHasCrostiniIntentFilters) {
  std::vector<std::string> mime_types = {"text/csv", "text/plain"};

  // Set up the test Crostini app for our desired mime types.
  vm_tools::apps::App app;
  for (std::string mime_type : mime_types) {
    app.add_mime_types(mime_type);
  }
  app.set_desktop_file_id("app_id");
  vm_tools::apps::App::LocaleString::Entry* entry =
      app.mutable_name()->add_values();
  entry->set_locale(std::string());
  entry->set_value("app_name");
  test_helper()->AddApp(app);

  // Get the app ID so that we can find the Crostini app in App Service later.
  std::string app_service_id = crostini::CrostiniTestHelper::GenerateAppId(
      app.desktop_file_id(), crostini::kCrostiniDefaultVmName,
      crostini::kCrostiniDefaultContainerName);

  // Retrieve the registered intent filter for the app in App Service.
  std::vector<std::unique_ptr<IntentFilter>> intent_filters;
  app_service_proxy()->AppRegistryCache().ForOneApp(
      app_service_id, [&intent_filters](const AppUpdate& update) {
        for (auto& intent_filter : update.IntentFilters()) {
          intent_filters.push_back(std::move(intent_filter));
        }
      });

  EXPECT_EQ(intent_filters.size(), 1U);
  std::unique_ptr<IntentFilter> intent_filter = std::move(intent_filters[0]);
  EXPECT_EQ(intent_filter->conditions.size(), 2U);

  // Check that the filter has the correct action type.
  {
    const Condition* condition = intent_filter->conditions[0].get();
    ASSERT_EQ(condition->condition_type, ConditionType::kAction);
    EXPECT_EQ(condition->condition_values.size(), 1U);
    ASSERT_EQ(condition->condition_values[0]->match_type,
              PatternMatchType::kLiteral);
    ASSERT_EQ(condition->condition_values[0]->value,
              apps_util::kIntentActionView);
  }

  // Check that the filter has all our mime types.
  {
    const Condition* condition = intent_filter->conditions[1].get();
    ASSERT_EQ(condition->condition_type, ConditionType::kFile);
    EXPECT_EQ(condition->condition_values.size(), 2U);
    ASSERT_EQ(condition->condition_values[0]->value, mime_types[0]);
    ASSERT_EQ(condition->condition_values[1]->value, mime_types[1]);
  }
}

TEST_F(CrostiniAppsTest, AppReadinessUpdatesWhenCrostiniDisabled) {
  // Install a Crostini app.
  vm_tools::apps::App app;
  app.set_desktop_file_id("app_id");
  vm_tools::apps::App::LocaleString::Entry* entry =
      app.mutable_name()->add_values();
  entry->set_locale(std::string());
  entry->set_value("app_name");
  test_helper()->AddApp(app);

  // Get the app ID so that we can find the Crostini app in App Service later.
  std::string app_service_id = crostini::CrostiniTestHelper::GenerateAppId(
      app.desktop_file_id(), crostini::kCrostiniDefaultVmName,
      crostini::kCrostiniDefaultContainerName);

  // Check that the app is ready.
  apps::Readiness readiness_before;
  app_service_proxy()->AppRegistryCache().ForOneApp(
      app_service_id, [&readiness_before](const AppUpdate& update) {
        readiness_before = update.Readiness();
      });
  ASSERT_EQ(readiness_before, Readiness::kReady);

  // Disable Crostini. This call uninstalls all Crostini apps.
  guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile())
      ->ClearApplicationList(guest_os::VmType::TERMINA,
                             crostini::kCrostiniDefaultVmName, "");

  // Check that the app is now disabled.
  apps::Readiness readiness_after;
  app_service_proxy()->AppRegistryCache().ForOneApp(
      app_service_id, [&readiness_after](const AppUpdate& update) {
        readiness_after = update.Readiness();
      });
  ASSERT_EQ(readiness_after, Readiness::kUninstalledByUser);
}

}  // namespace apps
