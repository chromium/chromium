// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"

#include <stddef.h>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/ash/crostini/crostini_pref_names.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/guest_os/guest_os_pref_names.h"
#include "chrome/browser/ash/plugin_vm/fake_plugin_vm_features.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_test_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using vm_tools::apps::App;
using vm_tools::apps::ApplicationList;

namespace guest_os {

class GuestOsRegistryServiceTest : public testing::Test {
 public:
  GuestOsRegistryServiceTest() : crostini_test_helper_(&profile_) {
    RecreateService();
  }

  GuestOsRegistryServiceTest(const GuestOsRegistryServiceTest&) = delete;
  GuestOsRegistryServiceTest& operator=(const GuestOsRegistryServiceTest&) =
      delete;

 protected:
  void RecreateService() {
    service_.reset(nullptr);
    service_ = std::make_unique<GuestOsRegistryService>(&profile_);
    service_->SetClockForTesting(&test_clock_);
  }

  class Observer : public GuestOsRegistryService::Observer {
   public:
    MOCK_METHOD5(OnRegistryUpdated,
                 void(GuestOsRegistryService*,
                      VmType,
                      const std::vector<std::string>&,
                      const std::vector<std::string>&,
                      const std::vector<std::string>&));
    MOCK_METHOD3(OnAppLastLaunchTimeUpdated,
                 void(VmType, const std::string&, const base::Time&));
  };

  guest_os::GuestOsRegistryService* service() { return service_.get(); }
  TestingProfile* profile() { return &profile_; }

  std::vector<std::string> GetRegisteredAppIds() {
    std::vector<std::string> result;
    for (const auto& pair : service_->GetAllRegisteredApps()) {
      result.emplace_back(pair.first);
    }
    return result;
  }

  std::vector<std::string> GetEnabledAppIds() {
    std::vector<std::string> result;
    for (const auto& pair : service_->GetEnabledApps()) {
      result.emplace_back(pair.first);
    }
    return result;
  }

 protected:
  base::SimpleTestClock test_clock_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  crostini::CrostiniTestHelper crostini_test_helper_;

  std::unique_ptr<GuestOsRegistryService> service_;
};

TEST_F(GuestOsRegistryServiceTest, SetAndGetRegistration) {
  std::string desktop_file_id = "vim";
  auto vm_type = vm_tools::apps::VmType::TERMINA;
  std::string vm_name = "awesomevm";
  std::string container_name = "awesomecontainer";
  std::map<std::string, std::string> name = {{"", "Vim"}};
  std::map<std::string, std::string> comment = {{"", "Edit text files"}};
  std::map<std::string, std::set<std::string>> keywords = {
      {"", {"very", "awesome"}}};
  std::set<std::string> mime_types = {"text/plain", "text/x-python"};
  bool no_display = true;
  std::string exec = "execName --extra-arg=true";
  std::string executable_file_name = "execName";
  std::string package_id =
      "vim;2:8.0.0197-4+deb9u1;amd64;installed:debian-stable";

  std::string app_id = crostini::CrostiniTestHelper::GenerateAppId(
      desktop_file_id, vm_name, container_name);
  EXPECT_FALSE(service()->GetRegistration(app_id).has_value());

  ApplicationList app_list;
  app_list.set_vm_type(vm_type);
  app_list.set_vm_name(vm_name);
  app_list.set_container_name(container_name);

  App* app = app_list.add_apps();
  app->set_desktop_file_id(desktop_file_id);
  app->set_no_display(no_display);
  app->set_exec(exec);
  app->set_executable_file_name(executable_file_name);
  app->set_package_id(package_id);

  for (const auto& localized_name : name) {
    App::LocaleString::Entry* entry = app->mutable_name()->add_values();
    entry->set_locale(localized_name.first);
    entry->set_value(localized_name.second);
  }

  for (const auto& localized_comment : comment) {
    App::LocaleString::Entry* entry = app->mutable_comment()->add_values();
    entry->set_locale(localized_comment.first);
    entry->set_value(localized_comment.second);
  }

  for (const auto& localized_keywords : keywords) {
    auto* keywords_with_locale = app->mutable_keywords()->add_values();
    keywords_with_locale->set_locale(localized_keywords.first);
    for (const auto& localized_keyword : localized_keywords.second) {
      keywords_with_locale->add_value(localized_keyword);
    }
  }

  for (const std::string& mime_type : mime_types)
    app->add_mime_types(mime_type);

  service()->UpdateApplicationList(app_list);
  std::optional<GuestOsRegistryService::Registration> result =
      service()->GetRegistration(app_id);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->DesktopFileId(), desktop_file_id);
  EXPECT_EQ(result->VmType(), std::make_optional(VmType::TERMINA));
  EXPECT_EQ(result->VmName(), vm_name);
  EXPECT_EQ(result->ContainerName(), container_name);
  EXPECT_EQ(result->Name(), name[""]);
  EXPECT_EQ(result->Keywords(), keywords[""]);
  EXPECT_EQ(result->MimeTypes(), mime_types);
  EXPECT_EQ(result->NoDisplay(), no_display);
  EXPECT_EQ(result->Exec(), exec);
  EXPECT_EQ(result->ExecutableFileName(), executable_file_name);
  EXPECT_EQ(result->PackageId(), package_id);
}

TEST_F(GuestOsRegistryServiceTest, Observer) {
  ApplicationList app_list =
      crostini::CrostiniTestHelper::BasicAppList("app 1", "vm", "container");
  *app_list.add_apps() = crostini::CrostiniTestHelper::BasicApp("app 2");
  *app_list.add_apps() = crostini::CrostiniTestHelper::BasicApp("app 3");
  std::string app_id_1 =
      crostini::CrostiniTestHelper::GenerateAppId("app 1", "vm", "container");
  std::string app_id_2 =
      crostini::CrostiniTestHelper::GenerateAppId("app 2", "vm", "container");
  std::string app_id_3 =
      crostini::CrostiniTestHelper::GenerateAppId("app 3", "vm", "container");
  std::string app_id_4 =
      crostini::CrostiniTestHelper::GenerateAppId("app 4", "vm", "container");

  Observer observer;
  service()->AddObserver(&observer);
  EXPECT_CALL(
      observer,
      OnRegistryUpdated(
          service(), VmType::TERMINA, testing::IsEmpty(), testing::IsEmpty(),
          testing::UnorderedElementsAre(app_id_1, app_id_2, app_id_3)));
  service()->UpdateApplicationList(app_list);

  // Rename desktop file for "app 2" to "app 4" (deletion+insertion)
  app_list.mutable_apps(1)->set_desktop_file_id("app 4");
  // Rename name for "app 3" to "banana"
  app_list.mutable_apps(2)->mutable_name()->mutable_values(0)->set_value(
      "banana");
  EXPECT_CALL(observer, OnRegistryUpdated(service(), VmType::TERMINA,
                                          testing::ElementsAre(app_id_3),
                                          testing::ElementsAre(app_id_2),
                                          testing::ElementsAre(app_id_4)));
  service()->UpdateApplicationList(app_list);
}

TEST_F(GuestOsRegistryServiceTest, ObserverForPvmDefault) {
  ApplicationList app_list = crostini::CrostiniTestHelper::BasicAppList(
      "app 1", "PvmDefault", "container");
  app_list.set_vm_type(vm_tools::apps::PLUGIN_VM);
  std::string app_id_1 = crostini::CrostiniTestHelper::GenerateAppId(
      "app 1", "PvmDefault", "container");

  Observer observer;
  service()->AddObserver(&observer);

  // Observers should be called when apps are added or updated.
  EXPECT_CALL(observer,
              OnRegistryUpdated(service(), VmType::PLUGIN_VM,
                                testing::IsEmpty(), testing::IsEmpty(),
                                testing::UnorderedElementsAre(app_id_1)))
      .Times(1);
  service()->UpdateApplicationList(app_list);

  // Observers should be called when apps are removed.
  EXPECT_CALL(observer,
              OnRegistryUpdated(
                  service(), VmType::PLUGIN_VM, testing::IsEmpty(),
                  testing::UnorderedElementsAre(app_id_1), testing::IsEmpty()))
      .Times(1);
  service()->ClearApplicationList(VmType::PLUGIN_VM, "PvmDefault", "");
}

TEST_F(GuestOsRegistryServiceTest, ZeroAppsInstalledHistogram) {
  base::HistogramTester histogram_tester;

  RecreateService();
}

TEST_F(GuestOsRegistryServiceTest, NAppsInstalledHistogram) {
  base::HistogramTester histogram_tester;

  // Set up an app list with the expected number of apps.
  ApplicationList app_list =
      crostini::CrostiniTestHelper::BasicAppList("app 0", "vm", "container");
  *app_list.add_apps() = crostini::CrostiniTestHelper::BasicApp("app 1");
  *app_list.add_apps() = crostini::CrostiniTestHelper::BasicApp("app 2");
  *app_list.add_apps() = crostini::CrostiniTestHelper::BasicApp("app 3");

  // Add apps that should not be counted.
  App app4 = crostini::CrostiniTestHelper::BasicApp("no display app 4");
  app4.set_no_display(true);
  *app_list.add_apps() = app4;

  App app5 = crostini::CrostiniTestHelper::BasicApp("no display app 5");
  app5.set_no_display(true);
  *app_list.add_apps() = app5;

  // Update the list of apps so that they can be counted.
  service()->UpdateApplicationList(app_list);

  RecreateService();
}

TEST_F(GuestOsRegistryServiceTest, PluginVmAppsInstalledHistogram) {
  base::HistogramTester histogram_tester;

  plugin_vm::PluginVmTestHelper test_helper(profile());
  test_helper.AllowPluginVm();

  // Plugin VM needs to be enabled before we start counting.
  RecreateService();

  test_helper.EnablePluginVm();
  // Set up an app list with the expected number of apps.
  ApplicationList app_list = crostini::CrostiniTestHelper::BasicAppList(
      "app 1", "PvmDefault", "container");
  *app_list.add_apps() = crostini::CrostiniTestHelper::BasicApp("app 2");
  app_list.set_vm_type(vm_tools::apps::PLUGIN_VM);
  service()->UpdateApplicationList(app_list);

  RecreateService();
}

TEST_F(GuestOsRegistryServiceTest, InstallAndLaunchTime) {
  ApplicationList app_list =
      crostini::CrostiniTestHelper::BasicAppList("app", "vm", "container");
  std::string app_id =
      crostini::CrostiniTestHelper::GenerateAppId("app", "vm", "container");
  test_clock_.Advance(base::Hours(1));

  Observer observer;
  service()->AddObserver(&observer);
  EXPECT_CALL(observer, OnRegistryUpdated(
                            service(), VmType::TERMINA, testing::IsEmpty(),
                            testing::IsEmpty(), testing::ElementsAre(app_id)));
  service()->UpdateApplicationList(app_list);

  std::optional<GuestOsRegistryService::Registration> result =
      service()->GetRegistration(app_id);
  base::Time install_time = test_clock_.Now();
  EXPECT_EQ(result->InstallTime(), install_time);
  EXPECT_EQ(result->LastLaunchTime(), base::Time());

  // UpdateApplicationList with nothing changed. Times shouldn't be updated and
  // the observer shouldn't fire.
  test_clock_.Advance(base::Hours(1));
  EXPECT_CALL(observer, OnRegistryUpdated(_, _, _, _, _)).Times(0);
  service()->UpdateApplicationList(app_list);
  result = service()->GetRegistration(app_id);
  EXPECT_EQ(result->InstallTime(), install_time);
  EXPECT_EQ(result->LastLaunchTime(), base::Time());

  // Launch the app
  test_clock_.Advance(base::Hours(1));
  EXPECT_CALL(observer,
              OnAppLastLaunchTimeUpdated(VmType::TERMINA, app_id,
                                         base::Time() + base::Hours(3)));
  service()->AppLaunched(app_id);
  result = service()->GetRegistration(app_id);
  EXPECT_EQ(result->InstallTime(), install_time);
  EXPECT_EQ(result->LastLaunchTime(), base::Time() + base::Hours(3));

  // The install time shouldn't change if fields change.
  test_clock_.Advance(base::Hours(1));
  app_list.mutable_apps(0)->set_no_display(true);
  EXPECT_CALL(observer,
              OnRegistryUpdated(service(), VmType::TERMINA,
                                testing::ElementsAre(app_id),
                                testing::IsEmpty(), testing::IsEmpty()));
  service()->UpdateApplicationList(app_list);
  result = service()->GetRegistration(app_id);
  EXPECT_EQ(result->InstallTime(), install_time);
  EXPECT_EQ(result->LastLaunchTime(), base::Time() + base::Hours(3));
}

// Test that UpdateApplicationList doesn't clobber apps from different VMs or
// containers.
TEST_F(GuestOsRegistryServiceTest, MultipleContainers) {
  service()->UpdateApplicationList(
      crostini::CrostiniTestHelper::BasicAppList("app", "vm 1", "container 1"));
  service()->UpdateApplicationList(
      crostini::CrostiniTestHelper::BasicAppList("app", "vm 1", "container 2"));
  service()->UpdateApplicationList(
      crostini::CrostiniTestHelper::BasicAppList("app", "vm 2", "container 1"));
  std::string app_id_1 =
      crostini::CrostiniTestHelper::GenerateAppId("app", "vm 1", "container 1");
  std::string app_id_2 =
      crostini::CrostiniTestHelper::GenerateAppId("app", "vm 1", "container 2");
  std::string app_id_3 =
      crostini::CrostiniTestHelper::GenerateAppId("app", "vm 2", "container 1");

  EXPECT_THAT(GetRegisteredAppIds(),
              testing::UnorderedElementsAre(app_id_1, app_id_2, app_id_3));

  // Clobber app_id_2
  service()->UpdateApplicationList(crostini::CrostiniTestHelper::BasicAppList(
      "app 2", "vm 1", "container 2"));
  std::string new_app_id = crostini::CrostiniTestHelper::GenerateAppId(
      "app 2", "vm 1", "container 2");

  EXPECT_THAT(GetRegisteredAppIds(),
              testing::UnorderedElementsAre(app_id_1, app_id_3, new_app_id));
}

// Test that ClearApplicationList works, and only removes apps from the
// specified VM.
TEST_F(GuestOsRegistryServiceTest, ClearApplicationList) {
  service()->UpdateApplicationList(
      crostini::CrostiniTestHelper::BasicAppList("app", "vm 1", "container 1"));
  service()->UpdateApplicationList(
      crostini::CrostiniTestHelper::BasicAppList("app", "vm 1", "container 2"));
  ApplicationList app_list =
      crostini::CrostiniTestHelper::BasicAppList("app", "vm 2", "container 1");
  *app_list.add_apps() = crostini::CrostiniTestHelper::BasicApp("app 2");
  service()->UpdateApplicationList(app_list);
  std::string app_id_1 =
      crostini::CrostiniTestHelper::GenerateAppId("app", "vm 1", "container 1");
  std::string app_id_2 =
      crostini::CrostiniTestHelper::GenerateAppId("app", "vm 1", "container 2");
  std::string app_id_3 =
      crostini::CrostiniTestHelper::GenerateAppId("app", "vm 2", "container 1");
  std::string app_id_4 = crostini::CrostiniTestHelper::GenerateAppId(
      "app 2", "vm 2", "container 1");

  EXPECT_THAT(
      GetRegisteredAppIds(),
      testing::UnorderedElementsAre(app_id_1, app_id_2, app_id_3, app_id_4));

  service()->ClearApplicationList(VmType::TERMINA, "vm 2", "");

  EXPECT_THAT(GetRegisteredAppIds(),
              testing::UnorderedElementsAre(app_id_1, app_id_2));
}

TEST_F(GuestOsRegistryServiceTest, IsScaledReturnFalseWhenNotSet) {
  std::string app_id =
      crostini::CrostiniTestHelper::GenerateAppId("app", "vm", "container");
  ApplicationList app_list =
      crostini::CrostiniTestHelper::BasicAppList("app", "vm", "container");
  service()->UpdateApplicationList(app_list);
  std::optional<GuestOsRegistryService::Registration> registration =
      service()->GetRegistration(app_id);
  EXPECT_TRUE(registration.has_value());
  EXPECT_FALSE(registration.value().IsScaled());
}

TEST_F(GuestOsRegistryServiceTest, SetScaledWorks) {
  std::string app_id =
      crostini::CrostiniTestHelper::GenerateAppId("app", "vm", "container");
  ApplicationList app_list =
      crostini::CrostiniTestHelper::BasicAppList("app", "vm", "container");
  service()->UpdateApplicationList(app_list);
  service()->SetAppScaled(app_id, true);
  std::optional<GuestOsRegistryService::Registration> registration =
      service()->GetRegistration(app_id);
  EXPECT_TRUE(registration.has_value());
  EXPECT_TRUE(registration.value().IsScaled());
  service()->SetAppScaled(app_id, false);
  registration = service()->GetRegistration(app_id);
  EXPECT_TRUE(registration.has_value());
  EXPECT_FALSE(registration.value().IsScaled());
}

TEST_F(GuestOsRegistryServiceTest, SetAndGetRegistrationKeywords) {
  std::map<std::string, std::set<std::string>> keywords = {
      {"", {"very", "awesome"}},
      {"fr", {"sum", "ward"}},
      {"ge", {"extra", "average"}},
      {"te", {"not", "terrible"}}};
  std::string app_id =
      crostini::CrostiniTestHelper::GenerateAppId("app", "vm", "container");
  ApplicationList app_list =
      crostini::CrostiniTestHelper::BasicAppList("app", "vm", "container");

  for (const auto& localized_keywords : keywords) {
    auto* keywords_with_locale =
        app_list.mutable_apps(0)->mutable_keywords()->add_values();
    keywords_with_locale->set_locale(localized_keywords.first);
    for (const auto& localized_keyword : localized_keywords.second) {
      keywords_with_locale->add_value(localized_keyword);
    }
  }
  service()->UpdateApplicationList(app_list);

  std::optional<GuestOsRegistryService::Registration> result =
      service()->GetRegistration(app_id);
  g_browser_process->SetApplicationLocale("");
  EXPECT_EQ(result->Keywords(), keywords[""]);
  g_browser_process->SetApplicationLocale("fr");
  EXPECT_EQ(result->Keywords(), keywords["fr"]);
  g_browser_process->SetApplicationLocale("ge");
  EXPECT_EQ(result->Keywords(), keywords["ge"]);
  g_browser_process->SetApplicationLocale("te");
  EXPECT_EQ(result->Keywords(), keywords["te"]);
}

TEST_F(GuestOsRegistryServiceTest, SetAndGetRegistrationExec) {
  std::string exec = "execName --extra-arg=true";
  std::string app_id_valid_exec =
      crostini::CrostiniTestHelper::GenerateAppId("app", "vm", "container");
  std::string app_id_no_exec =
      crostini::CrostiniTestHelper::GenerateAppId("noExec", "vm", "container");
  ApplicationList app_list =
      crostini::CrostiniTestHelper::BasicAppList("app", "vm", "container");
  *app_list.add_apps() = crostini::CrostiniTestHelper::BasicApp("noExec");

  app_list.mutable_apps(0)->set_exec(exec);
  service()->UpdateApplicationList(app_list);

  std::optional<GuestOsRegistryService::Registration> result_valid_exec =
      service()->GetRegistration(app_id_valid_exec);
  std::optional<GuestOsRegistryService::Registration> result_no_exec =
      service()->GetRegistration(app_id_no_exec);
  EXPECT_EQ(result_valid_exec->Exec(), exec);
  EXPECT_EQ(result_no_exec->Exec(), "");
}

TEST_F(GuestOsRegistryServiceTest, SetAndGetRegistrationExecutableFileName) {
  std::string executable_file_name = "myExec";
  std::string app_id_valid_exec =
      crostini::CrostiniTestHelper::GenerateAppId("app", "vm", "container");
  std::string app_id_no_exec =
      crostini::CrostiniTestHelper::GenerateAppId("noExec", "vm", "container");
  ApplicationList app_list =
      crostini::CrostiniTestHelper::BasicAppList("app", "vm", "container");
  *app_list.add_apps() = crostini::CrostiniTestHelper::BasicApp("noExec");

  app_list.mutable_apps(0)->set_executable_file_name(executable_file_name);
  service()->UpdateApplicationList(app_list);

  std::optional<GuestOsRegistryService::Registration> result_valid_exec =
      service()->GetRegistration(app_id_valid_exec);
  std::optional<GuestOsRegistryService::Registration> result_no_exec =
      service()->GetRegistration(app_id_no_exec);
  EXPECT_EQ(result_valid_exec->ExecutableFileName(), executable_file_name);
  EXPECT_EQ(result_no_exec->ExecutableFileName(), "");
}

TEST_F(GuestOsRegistryServiceTest, SetAndGetPackageId) {
  std::string package_id =
      "vim;2:8.0.0197-4+deb9u1;amd64;installed:debian-stable";
  std::string app_id_valid_package_id =
      crostini::CrostiniTestHelper::GenerateAppId("app", "vm", "container");
  std::string app_id_no_package_id =
      crostini::CrostiniTestHelper::GenerateAppId("noPackageId", "vm",
                                                  "container");
  ApplicationList app_list =
      crostini::CrostiniTestHelper::BasicAppList("app", "vm", "container");
  *app_list.add_apps() = crostini::CrostiniTestHelper::BasicApp("noPackageId");

  app_list.mutable_apps(0)->set_package_id(package_id);
  service()->UpdateApplicationList(app_list);

  std::optional<GuestOsRegistryService::Registration> result_valid_package_id =
      service()->GetRegistration(app_id_valid_package_id);
  std::optional<GuestOsRegistryService::Registration> result_no_package_id =
      service()->GetRegistration(app_id_no_package_id);
  EXPECT_EQ(result_valid_package_id->PackageId(), package_id);
  EXPECT_EQ(result_no_package_id->PackageId(), "");
}

TEST_F(GuestOsRegistryServiceTest, GetEnabledApps) {
  crostini::FakeCrostiniFeatures fake_crostini_features;
  plugin_vm::FakePluginVmFeatures fake_plugin_vm_features;

  ApplicationList crostini_list;
  crostini_list.set_vm_type(VmType::TERMINA);
  crostini_list.set_vm_name("termina");
  crostini_list.set_container_name("penguin");
  *crostini_list.add_apps() = crostini::CrostiniTestHelper::BasicApp("c");
  std::string c =
      crostini::CrostiniTestHelper::GenerateAppId("c", "termina", "penguin");
  service()->UpdateApplicationList(crostini_list);

  ApplicationList plugin_vm_list;
  plugin_vm_list.set_vm_type(VmType::PLUGIN_VM);
  plugin_vm_list.set_vm_name("PvmDefault");
  plugin_vm_list.set_container_name("penguin");
  *plugin_vm_list.add_apps() = crostini::CrostiniTestHelper::BasicApp("p");
  std::string p =
      crostini::CrostiniTestHelper::GenerateAppId("p", "PvmDefault", "penguin");
  service()->UpdateApplicationList(plugin_vm_list);

  // All enabled.
  fake_crostini_features.set_enabled(true);
  fake_plugin_vm_features.set_enabled(true);
  EXPECT_THAT(GetRegisteredAppIds(), testing::UnorderedElementsAre(p, c));
  EXPECT_THAT(GetEnabledAppIds(), testing::UnorderedElementsAre(p, c));

  // Crostini disabled.
  fake_crostini_features.set_enabled(false);
  fake_plugin_vm_features.set_enabled(true);
  EXPECT_THAT(GetRegisteredAppIds(), testing::UnorderedElementsAre(p, c));
  EXPECT_THAT(GetEnabledAppIds(), testing::UnorderedElementsAre(p));

  // Plugin VM disabled.
  fake_crostini_features.set_enabled(true);
  fake_plugin_vm_features.set_enabled(false);
  EXPECT_THAT(GetRegisteredAppIds(), testing::UnorderedElementsAre(p, c));
  EXPECT_THAT(GetEnabledAppIds(), testing::UnorderedElementsAre(c));

  // All disabled.
  fake_crostini_features.set_enabled(false);
  fake_plugin_vm_features.set_enabled(false);
  EXPECT_THAT(GetRegisteredAppIds(), testing::UnorderedElementsAre(p, c));
  EXPECT_THAT(GetEnabledAppIds(), testing::IsEmpty());
}

TEST_F(GuestOsRegistryServiceTest, PluginVmNameSuffix) {
  ApplicationList crostini_list;
  crostini_list.set_vm_type(VmType::TERMINA);
  crostini_list.set_vm_name("termina");
  crostini_list.set_container_name("penguin");
  *crostini_list.add_apps() = crostini::CrostiniTestHelper::BasicApp("c");
  std::string c =
      crostini::CrostiniTestHelper::GenerateAppId("c", "termina", "penguin");
  service()->UpdateApplicationList(crostini_list);

  ApplicationList plugin_vm_list;
  plugin_vm_list.set_vm_type(VmType::PLUGIN_VM);
  plugin_vm_list.set_vm_name("PvmDefault");
  plugin_vm_list.set_container_name("penguin");
  *plugin_vm_list.add_apps() = crostini::CrostiniTestHelper::BasicApp("p");
  std::string p =
      crostini::CrostiniTestHelper::GenerateAppId("p", "PvmDefault", "penguin");
  service()->UpdateApplicationList(plugin_vm_list);

  // Crostini apps have name unchanged, PluginVM has ' (Windows)' suffix.
  EXPECT_EQ("c", service()->GetRegistration(c)->Name());
  EXPECT_EQ("p (Windows)", service()->GetRegistration(p)->Name());
}

}  // namespace guest_os
