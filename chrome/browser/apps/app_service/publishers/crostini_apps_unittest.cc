// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/crostini_apps.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service.h"
#include "chrome/browser/ash/guest_os/guest_os_mime_types_service_factory.h"
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

  guest_os::GuestOsMimeTypesService* mime_types_service() {
    return mime_types_service_;
  }

  void SetUp() override {
    ash::CiceroneClient::InitializeFake();
    profile_ = std::make_unique<TestingProfile>();
    app_service_proxy_ = AppServiceProxyFactory::GetForProfile(profile_.get());
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile_.get());
    test_helper_ =
        std::make_unique<crostini::CrostiniTestHelper>(profile_.get());
    test_helper()->ReInitializeAppServiceIntegration();
    mime_types_service_ =
        guest_os::GuestOsMimeTypesServiceFactory::GetForProfile(profile());
  }

  void TearDown() override {
    test_helper_.reset();
    profile_.reset();
    ash::CiceroneClient::Shutdown();
  }

  // Set up the test Crostini app for our desired mime types.
  std::string AddCrostiniAppWithMimeTypes(std::string app_id,
                                          std::string app_name,
                                          std::vector<std::string> mime_types) {
    vm_tools::apps::App app;
    for (std::string mime_type : mime_types) {
      app.add_mime_types(mime_type);
    }
    app.set_desktop_file_id(app_id);
    vm_tools::apps::App::LocaleString::Entry* entry =
        app.mutable_name()->add_values();
    entry->set_locale(std::string());
    entry->set_value(app_name);
    test_helper()->AddApp(app);

    return crostini::CrostiniTestHelper::GenerateAppId(
        app.desktop_file_id(), crostini::kCrostiniDefaultVmName,
        crostini::kCrostiniDefaultContainerName);
  }

  // Get the registered intent filter for the app in App Service.
  std::vector<std::unique_ptr<IntentFilter>> GetIntentFiltersForApp(
      std::string app_id) {
    std::vector<std::unique_ptr<IntentFilter>> intent_filters;
    app_service_proxy()->AppRegistryCache().ForOneApp(
        app_id, [&intent_filters](const AppUpdate& update) {
          for (auto& intent_filter : update.IntentFilters()) {
            intent_filters.push_back(std::move(intent_filter));
          }
        });
    return intent_filters;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<crostini::CrostiniTestHelper> test_helper_;
  AppServiceProxy* app_service_proxy_ = nullptr;
  guest_os::GuestOsMimeTypesService* mime_types_service_;
};

TEST_F(CrostiniAppsTest, AppServiceHasCrostiniIntentFilters) {
  std::vector<std::string> mime_types = {"text/csv", "text/html"};

  std::string app_id =
      AddCrostiniAppWithMimeTypes("app_id", "app_name", mime_types);
  std::vector<std::unique_ptr<IntentFilter>> intent_filters =
      GetIntentFiltersForApp(app_id);

  ASSERT_EQ(intent_filters.size(), 1U);
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

  // Check that the filter has all our mime types. Realistically, the filter
  // would also have the extension equivalents of the mime types too if there
  // were mime/ extension mappings in prefs.
  {
    const Condition* condition = intent_filters[0]->conditions[1].get();
    ASSERT_EQ(condition->condition_type, ConditionType::kFile);
    EXPECT_EQ(condition->condition_values.size(), 2U);
    ASSERT_EQ(condition->condition_values[0]->value, mime_types[0]);
    ASSERT_EQ(condition->condition_values[1]->value, mime_types[1]);
  }
}

TEST_F(CrostiniAppsTest, AppReadinessUpdatesWhenCrostiniDisabled) {
  // Install a Crostini app.
  std::string app_id = AddCrostiniAppWithMimeTypes("app_id", "app_name", {});

  // Check that the app is ready.
  apps::Readiness readiness_before;
  app_service_proxy()->AppRegistryCache().ForOneApp(
      app_id, [&readiness_before](const AppUpdate& update) {
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
      app_id, [&readiness_after](const AppUpdate& update) {
        readiness_after = update.Readiness();
      });
  ASSERT_EQ(readiness_after, Readiness::kUninstalledByUser);
}

TEST_F(CrostiniAppsTest, CrostiniIntentFilterHasExtensionsFromPrefs) {
  base::test::ScopedFeatureList scoped_feature_list{
      ash::features::kGuestOsFileTasksUseAppService};

  std::string mime_type = "test/mime1";
  std::string extension = "test_extension";

  // Update dictionary to map the extension to mime type.
  vm_tools::apps::MimeTypes mime_types_list;
  mime_types_list.set_vm_name("termina");
  mime_types_list.set_container_name("penguin");
  (*mime_types_list.mutable_mime_type_mappings())[extension] = mime_type;
  mime_types_service()->UpdateMimeTypes(mime_types_list);

  // Create Crostini app and get its registered intent filters.
  std::string app_id =
      AddCrostiniAppWithMimeTypes("app_id", "app_name", {mime_type});
  std::vector<std::unique_ptr<IntentFilter>> intent_filters =
      GetIntentFiltersForApp(app_id);

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

  // Check that the filter has both mime type and its extension equivalent based
  // on what is recorded in prefs for GuestOS mime types.
  {
    const Condition* condition = intent_filter->conditions[1].get();
    ASSERT_EQ(condition->condition_type, ConditionType::kFile);
    EXPECT_EQ(condition->condition_values.size(), 2U);
    ASSERT_EQ(condition->condition_values[0]->value, mime_type);
    ASSERT_EQ(condition->condition_values[0]->match_type,
              PatternMatchType::kMimeType);
    ASSERT_EQ(condition->condition_values[1]->value, extension);
    ASSERT_EQ(condition->condition_values[1]->match_type,
              PatternMatchType::kFileExtension);
  }
}

TEST_F(CrostiniAppsTest, IntentFilterWithTextPlainAddsTextWildcardMimeType) {
  std::string normal_app_id = AddCrostiniAppWithMimeTypes(
      "normal_app_id", "normal_app_name", {"text/csv"});
  std::vector<std::unique_ptr<IntentFilter>> intent_filters =
      GetIntentFiltersForApp(normal_app_id);

  EXPECT_EQ(intent_filters.size(), 1U);
  EXPECT_EQ(intent_filters[0]->conditions.size(), 2U);

  // Check that the filter is unchanged because there isn't a "text/plain"
  // mime type condition.
  {
    const Condition* condition = intent_filters[0]->conditions[1].get();
    ASSERT_EQ(condition->condition_type, ConditionType::kFile);
    EXPECT_EQ(condition->condition_values.size(), 1U);
    ASSERT_EQ(condition->condition_values[0]->value, "text/csv");
  }

  std::string many_mimes_app_id = AddCrostiniAppWithMimeTypes(
      "many_mimes_app_id", "many_mimes_app_name",
      {"text/plain", "text/csv", "text/javascript", "video/mp4"});
  intent_filters = GetIntentFiltersForApp(many_mimes_app_id);

  EXPECT_EQ(intent_filters.size(), 1U);
  EXPECT_EQ(intent_filters[0]->conditions.size(), 2U);

  // Check that the filter has "text/*" to replace all the text mime types.
  {
    const Condition* condition = intent_filters[0]->conditions[1].get();
    ASSERT_EQ(condition->condition_type, ConditionType::kFile);
    EXPECT_EQ(condition->condition_values.size(), 2U);
    ASSERT_EQ(condition->condition_values[0]->value, "video/mp4");
    ASSERT_EQ(condition->condition_values[1]->value, "text/*");
  }
}

}  // namespace apps
