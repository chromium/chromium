// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/guest_os_apps.h"

#include <memory>
#include <vector>

#include "base/test/simple_test_clock.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/bruschetta/bruschetta_util.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/ash/guest_os/public/types.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

namespace {

// An app publisher (in the App Service sense) that inherits GuestOSApps and
// implements the necessary virtual functions.
class TestPublisher : public GuestOSApps {
 public:
  explicit TestPublisher(AppServiceProxy* proxy) : GuestOSApps(proxy) {}

 private:
  bool CouldBeAllowed() const override { return true; }

  apps::AppType AppType() const override { return AppType::kBruschetta; }
  guest_os::VmType VmType() const override {
    return guest_os::VmType::BRUSCHETTA;
  }

  // apps::AppPublisher overrides.
  void Launch(const std::string& app_id,
              int32_t event_flags,
              LaunchSource launch_source,
              WindowInfoPtr window_info) override {}

  void CreateAppOverrides(
      const guest_os::GuestOsRegistryService::Registration& registration,
      App* app) override {
    app->name = "override_name";
  }
};

}  // namespace

class GuestOSAppsTest : public testing::Test {
 public:
  GuestOSAppsTest()
      : task_environment_(content::BrowserTaskEnvironment::REAL_IO_THREAD) {}

  TestingProfile* profile() { return profile_.get(); }

  AppServiceProxy* app_service_proxy() { return app_service_proxy_; }

  guest_os::GuestOsRegistryService* registry() { return registry_.get(); }

  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    app_service_proxy_ = AppServiceProxyFactory::GetForProfile(profile_.get());
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile_.get());
    registry_ =
        guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile_.get());
    registry_->SetClockForTesting(&test_clock_);
    publisher_ = std::make_unique<TestPublisher>(app_service_proxy());
    publisher_->InitializeForTesting();
  }

  void TearDown() override {
    // TestPublisher must be torn down before TestingProfile because the latter
    // destroys the GuestOsRegistryService.
    publisher_.reset();
    profile_.reset();
  }

  // Adds the app to the registry and returns its app_id.
  std::string AddApp(const vm_tools::apps::App& app) {
    // Update the ApplicationList, this calls through GuestOSApps::CreateApp.
    vm_tools::apps::ApplicationList app_list;
    app_list.set_vm_name(bruschetta::kBruschettaVmName);
    app_list.set_container_name("test_container");
    *app_list.add_apps() = app;
    registry()->UpdateApplicationList(app_list);
    return registry()->GenerateAppId(app.desktop_file_id(),
                                     bruschetta::kBruschettaVmName,
                                     app_list.container_name());
  }

  // Adds an app with the specified mime_types and returns its app_id.
  std::string AddAppWithMimeTypes(const std::string& desktop_file_id,
                                  const std::string& app_name,
                                  const std::vector<std::string>& mime_types) {
    vm_tools::apps::App app;
    for (std::string mime_type : mime_types) {
      app.add_mime_types(mime_type);
    }
    app.set_desktop_file_id(desktop_file_id);
    vm_tools::apps::App::LocaleString::Entry* entry =
        app.mutable_name()->add_values();
    entry->set_locale(std::string());
    entry->set_value(app_name);
    return AddApp(app);
  }

  // Get the registered intent filters for the given app_id.
  std::vector<std::unique_ptr<IntentFilter>> GetIntentFiltersForApp(
      const std::string& app_id) {
    std::vector<std::unique_ptr<IntentFilter>> intent_filters;
    app_service_proxy()->AppRegistryCache().ForOneApp(
        app_id, [&intent_filters](const AppUpdate& update) {
          for (auto& intent_filter : update.IntentFilters()) {
            intent_filters.push_back(std::move(intent_filter));
          }
        });
    return intent_filters;
  }

  void UpdateMimeTypes(std::string container_name,
                       std::string extension,
                       std::string mime_type) {
    vm_tools::apps::MimeTypes mime_types_list;
    mime_types_list.set_vm_name(bruschetta::kBruschettaVmName);
    mime_types_list.set_container_name(container_name);
    (*mime_types_list.mutable_mime_type_mappings())[extension] = mime_type;
    auto* mime_types_service =
        guest_os::GuestOsMimeTypesServiceFactory::GetForProfile(profile());
    mime_types_service->UpdateMimeTypes(mime_types_list);
    task_environment_.RunUntilIdle();
  }

  base::SimpleTestClock& test_clock() { return test_clock_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<AppServiceProxy, DanglingUntriaged> app_service_proxy_ = nullptr;
  raw_ptr<guest_os::GuestOsRegistryService, DanglingUntriaged> registry_ =
      nullptr;
  std::unique_ptr<TestPublisher> publisher_;
  base::SimpleTestClock test_clock_;
};

TEST_F(GuestOSAppsTest, CreateApp) {
  // Create a test app.
  vm_tools::apps::App app;
  app.add_mime_types("text/plain");
  app.set_desktop_file_id("desktop_file_id");
  vm_tools::apps::App::LocaleString::Entry* entry =
      app.mutable_name()->add_values();
  entry->set_value("app_name");
  const std::string app_id = AddApp(app);

  // Get the AppUpdate from the registry and check its contents.
  bool seen = false;
  app_service_proxy()->AppRegistryCache().ForOneApp(
      app_id, [&seen](const AppUpdate& update) {
        seen = true;
        EXPECT_EQ(update.AppType(), AppType::kBruschetta);
        EXPECT_EQ(update.Readiness(), Readiness::kReady);
        EXPECT_EQ(update.Name(), "override_name")
            << "CreateAppOverrides should have changed the name.";
        EXPECT_EQ(update.InstallReason(), InstallReason::kUser);
        EXPECT_EQ(update.InstallSource(), InstallSource::kUnknown);
        EXPECT_TRUE(update.IconKey().has_value());
        EXPECT_TRUE(update.ShowInLauncher());
        EXPECT_TRUE(update.ShowInSearch());
        EXPECT_TRUE(update.ShowInShelf());
        EXPECT_EQ(bruschetta::kBruschettaVmName,
                  *update.Extra()->FindString("vm_name"));
        EXPECT_EQ("test_container",
                  *update.Extra()->FindString("container_name"));
        EXPECT_EQ("desktop_file_id",
                  *update.Extra()->FindString("desktop_file_id"));
        EXPECT_EQ("", *update.Extra()->FindString("exec"));
        EXPECT_EQ("", *update.Extra()->FindString("executable_file_name"));
        EXPECT_FALSE(update.Extra()->FindBool("no_display").value());
        EXPECT_FALSE(update.Extra()->FindBool("terminal").value());
        EXPECT_FALSE(update.Extra()->FindBool("scaled").value());
        EXPECT_EQ("", *update.Extra()->FindString("package_id"));
        EXPECT_EQ("", *update.Extra()->FindString("startup_wm_class"));
        EXPECT_FALSE(update.Extra()->FindBool("startup_notify").value());
      });
  EXPECT_TRUE(seen) << "Couldn't find test app in registry.";
}

TEST_F(GuestOSAppsTest, OnAppLastLaunchTimeUpdated) {
  // Create a test app.
  vm_tools::apps::App app;
  app.add_mime_types("text/plain");
  app.set_desktop_file_id("desktop_file_id");
  vm_tools::apps::App::LocaleString::Entry* entry =
      app.mutable_name()->add_values();
  entry->set_value("app_name");
  const std::string app_id = AddApp(app);

  // Get the AppUpdate from the registry and check its contents.
  app_service_proxy()->AppRegistryCache().ForOneApp(
      app_id, [](const AppUpdate& update) {
        EXPECT_EQ(update.LastLaunchTime(), base::Time());
      });

  test_clock().Advance(base::Hours(1));
  registry()->AppLaunched(app_id);

  // Get LastLaunchTime from the registry and check its contents.
  app_service_proxy()->AppRegistryCache().ForOneApp(
      app_id, [](const AppUpdate& update) {
        EXPECT_EQ(update.LastLaunchTime(), base::Time() + base::Hours(1));
      });
}

TEST_F(GuestOSAppsTest, AppServiceHasIntentFilters) {
  const std::vector<std::string> mime_types = {"text/csv", "text/html"};
  const std::string app_id =
      AddAppWithMimeTypes("desktop_file_id", "app_name", mime_types);
  const std::vector<std::unique_ptr<IntentFilter>> intent_filters =
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

TEST_F(GuestOSAppsTest, IntentFilterWithTextPlainAddsTextWildcardMimeType) {
  {
    const std::string normal_app_id = AddAppWithMimeTypes(
        "normal_desktop_file_id", "normal_app_name", {"text/csv"});
    const std::vector<std::unique_ptr<IntentFilter>> intent_filters =
        GetIntentFiltersForApp(normal_app_id);

    EXPECT_EQ(intent_filters.size(), 1U);
    EXPECT_EQ(intent_filters[0]->conditions.size(), 2U);

    // Check that the filter is unchanged because there isn't a "text/plain"
    // mime type condition.
    const Condition* condition = intent_filters[0]->conditions[1].get();
    ASSERT_EQ(condition->condition_type, ConditionType::kFile);
    EXPECT_EQ(condition->condition_values.size(), 1U);
    ASSERT_EQ(condition->condition_values[0]->value, "text/csv");
  }

  {
    const std::string many_mimes_app_id = AddAppWithMimeTypes(
        "many_mimes_desktop_file_id", "many_mimes_app_name",
        {"text/plain", "text/csv", "text/javascript", "video/mp4"});
    const std::vector<std::unique_ptr<IntentFilter>> intent_filters =
        GetIntentFiltersForApp(many_mimes_app_id);

    EXPECT_EQ(intent_filters.size(), 1U);
    EXPECT_EQ(intent_filters[0]->conditions.size(), 2U);

    // Check that the filter has "text/*" to replace all the text mime types.
    const Condition* condition = intent_filters[0]->conditions[1].get();
    ASSERT_EQ(condition->condition_type, ConditionType::kFile);
    EXPECT_EQ(condition->condition_values.size(), 2U);
    ASSERT_EQ(condition->condition_values[0]->value, "video/mp4");
    ASSERT_EQ(condition->condition_values[1]->value, "text/*");
  }
}

TEST_F(GuestOSAppsTest, IntentFilterHasExtensionsFromPrefs) {
  const std::string mime_type = "test/mime1";
  const std::string extension = "test_extension";
  UpdateMimeTypes("test_container", extension, mime_type);

  // Create app and get its registered intent filters.
  const std::string app_id =
      AddAppWithMimeTypes("desktop_file_id", "app_name", {mime_type});
  std::vector<std::unique_ptr<IntentFilter>> intent_filters =
      GetIntentFiltersForApp(app_id);

  ASSERT_EQ(intent_filters.size(), 1U);
  std::unique_ptr<IntentFilter> intent_filter = std::move(intent_filters[0]);
  ASSERT_EQ(intent_filter->conditions.size(), 2U);

  // Check that the filter has the correct action type.
  {
    const Condition* condition = intent_filter->conditions[0].get();
    EXPECT_EQ(condition->condition_type, ConditionType::kAction);
    ASSERT_EQ(condition->condition_values.size(), 1U);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              PatternMatchType::kLiteral);
    EXPECT_EQ(condition->condition_values[0]->value,
              apps_util::kIntentActionView);
  }

  // Check that the filter has both mime type and its extension equivalent based
  // on what is recorded in prefs for GuestOS mime types.
  {
    const Condition* condition = intent_filter->conditions[1].get();
    EXPECT_EQ(condition->condition_type, ConditionType::kFile);
    ASSERT_EQ(condition->condition_values.size(), 2U);
    EXPECT_EQ(condition->condition_values[0]->value, mime_type);
    EXPECT_EQ(condition->condition_values[0]->match_type,
              PatternMatchType::kMimeType);
    EXPECT_EQ(condition->condition_values[1]->value, extension);
    EXPECT_EQ(condition->condition_values[1]->match_type,
              PatternMatchType::kFileExtension);
  }
}

}  // namespace apps
