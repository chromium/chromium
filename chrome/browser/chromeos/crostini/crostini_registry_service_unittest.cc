// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"

#include <stddef.h>

#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/chromeos/crostini/crostini_test_helper.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using vm_tools::apps::App;
using vm_tools::apps::ApplicationList;

constexpr char kCrostiniAppsInstalledHistogram[] =
    "Crostini.AppsInstalledAtLogin";

namespace crostini {

class CrostiniRegistryServiceTest : public testing::Test {
 public:
  CrostiniRegistryServiceTest() = default;

  // testing::Test:
  void SetUp() override {
    SetCrostiniUIAllowedForTesting(true);

    CrostiniTestHelper::EnableCrostini(&profile_);

    RecreateService();
  }

  void TearDown() override { SetCrostiniUIAllowedForTesting(false); }

 protected:
  void RecreateService() {
    service_.reset(nullptr);
    service_ = std::make_unique<CrostiniRegistryService>(&profile_);
    service_->SetClockForTesting(&test_clock_);
  }

  class Observer : public CrostiniRegistryService::Observer {
   public:
    MOCK_METHOD4(OnRegistryUpdated,
                 void(CrostiniRegistryService*,
                      const std::vector<std::string>&,
                      const std::vector<std::string>&,
                      const std::vector<std::string>&));
  };

  std::string WindowIdForWMClass(const std::string& wm_class) {
    return "org.chromium.termina.wmclass." + wm_class;
  }

  CrostiniRegistryService* service() { return service_.get(); }

 protected:
  base::SimpleTestClock test_clock_;

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;

  std::unique_ptr<CrostiniRegistryService> service_;

  DISALLOW_COPY_AND_ASSIGN(CrostiniRegistryServiceTest);
};

TEST_F(CrostiniRegistryServiceTest, SetAndGetRegistration) {
  std::string desktop_file_id = "vim";
  std::string vm_name = "awesomevm";
  std::string container_name = "awesomecontainer";
  std::map<std::string, std::string> name = {{"", "Vim"}};
  std::map<std::string, std::string> comment = {{"", "Edit text files"}};
  std::set<std::string> mime_types = {"text/plain", "text/x-python"};
  bool no_display = true;

  std::string app_id = CrostiniTestHelper::GenerateAppId(
      desktop_file_id, vm_name, container_name);
  EXPECT_FALSE(service()->GetRegistration(app_id).has_value());

  ApplicationList app_list;
  app_list.set_vm_name(vm_name);
  app_list.set_container_name(container_name);

  App* app = app_list.add_apps();
  app->set_desktop_file_id(desktop_file_id);
  app->set_no_display(no_display);

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

  for (const std::string& mime_type : mime_types)
    app->add_mime_types(mime_type);

  service()->UpdateApplicationList(app_list);
  base::Optional<CrostiniRegistryService::Registration> result =
      service()->GetRegistration(app_id);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->DesktopFileId(), desktop_file_id);
  EXPECT_EQ(result->VmName(), vm_name);
  EXPECT_EQ(result->ContainerName(), container_name);
  EXPECT_EQ(result->Name(), name[""]);
  EXPECT_EQ(result->Comment(), comment[""]);
  EXPECT_EQ(result->MimeTypes(), mime_types);
  EXPECT_EQ(result->NoDisplay(), no_display);
}

TEST_F(CrostiniRegistryServiceTest, Observer) {
  ApplicationList app_list =
      CrostiniTestHelper::BasicAppList("app 1", "vm", "container");
  *app_list.add_apps() = CrostiniTestHelper::BasicApp("app 2");
  *app_list.add_apps() = CrostiniTestHelper::BasicApp("app 3");
  std::string app_id_1 =
      CrostiniTestHelper::GenerateAppId("app 1", "vm", "container");
  std::string app_id_2 =
      CrostiniTestHelper::GenerateAppId("app 2", "vm", "container");
  std::string app_id_3 =
      CrostiniTestHelper::GenerateAppId("app 3", "vm", "container");
  std::string app_id_4 =
      CrostiniTestHelper::GenerateAppId("app 4", "vm", "container");

  Observer observer;
  service()->AddObserver(&observer);
  EXPECT_CALL(observer,
              OnRegistryUpdated(
                  service(), testing::IsEmpty(), testing::IsEmpty(),
                  testing::UnorderedElementsAre(app_id_1, app_id_2, app_id_3)));
  service()->UpdateApplicationList(app_list);

  // Rename desktop file for "app 2" to "app 4" (deletion+insertion)
  app_list.mutable_apps(1)->set_desktop_file_id("app 4");
  // Rename name for "app 3" to "banana"
  app_list.mutable_apps(2)->mutable_name()->mutable_values(0)->set_value(
      "banana");
  EXPECT_CALL(observer,
              OnRegistryUpdated(service(), testing::ElementsAre(app_id_3),
                                testing::ElementsAre(app_id_2),
                                testing::ElementsAre(app_id_4)));
  service()->UpdateApplicationList(app_list);
}

TEST_F(CrostiniRegistryServiceTest, ZeroAppsInstalledHistogram) {
  base::HistogramTester histogram_tester;

  RecreateService();

  // Check that there are no apps installed.
  histogram_tester.ExpectUniqueSample(kCrostiniAppsInstalledHistogram, 0, 1);
}

TEST_F(CrostiniRegistryServiceTest, NAppsInstalledHistogram) {
  base::HistogramTester histogram_tester;

  // Set up an app list with the expected number of apps.
  ApplicationList app_list =
      CrostiniTestHelper::BasicAppList("app 0", "vm", "container");
  *app_list.add_apps() = CrostiniTestHelper::BasicApp("app 1");
  *app_list.add_apps() = CrostiniTestHelper::BasicApp("app 2");
  *app_list.add_apps() = CrostiniTestHelper::BasicApp("app 3");

  // Add apps that should not be counted.
  App app4 = CrostiniTestHelper::BasicApp("no display app 4");
  app4.set_no_display(true);
  *app_list.add_apps() = app4;

  App app5 = CrostiniTestHelper::BasicApp("no display app 5");
  app5.set_no_display(true);
  *app_list.add_apps() = app5;

  // Force the registry to have a prefs entry for the Terminal.
  service()->AppLaunched(kCrostiniTerminalId);

  // Update the list of apps so that they can be counted.
  service()->UpdateApplicationList(app_list);

  RecreateService();

  histogram_tester.ExpectUniqueSample(kCrostiniAppsInstalledHistogram, 4, 1);
}

TEST_F(CrostiniRegistryServiceTest, InstallAndLaunchTime) {
  ApplicationList app_list =
      CrostiniTestHelper::BasicAppList("app", "vm", "container");
  std::string app_id =
      CrostiniTestHelper::GenerateAppId("app", "vm", "container");
  test_clock_.Advance(base::TimeDelta::FromHours(1));

  Observer observer;
  service()->AddObserver(&observer);
  EXPECT_CALL(observer, OnRegistryUpdated(service(), testing::IsEmpty(),
                                          testing::IsEmpty(),
                                          testing::ElementsAre(app_id)));
  service()->UpdateApplicationList(app_list);

  base::Optional<CrostiniRegistryService::Registration> result =
      service()->GetRegistration(app_id);
  base::Time install_time = test_clock_.Now();
  EXPECT_EQ(result->InstallTime(), install_time);
  EXPECT_EQ(result->LastLaunchTime(), base::Time());

  // UpdateApplicationList with nothing changed. Times shouldn't be updated and
  // the observer shouldn't fire.
  test_clock_.Advance(base::TimeDelta::FromHours(1));
  EXPECT_CALL(observer, OnRegistryUpdated(_, _, _, _)).Times(0);
  service()->UpdateApplicationList(app_list);
  result = service()->GetRegistration(app_id);
  EXPECT_EQ(result->InstallTime(), install_time);
  EXPECT_EQ(result->LastLaunchTime(), base::Time());

  // Launch the app
  test_clock_.Advance(base::TimeDelta::FromHours(1));
  service()->AppLaunched(app_id);
  result = service()->GetRegistration(app_id);
  EXPECT_EQ(result->InstallTime(), install_time);
  EXPECT_EQ(result->LastLaunchTime(),
            base::Time() + base::TimeDelta::FromHours(3));

  // The install time shouldn't change if fields change.
  test_clock_.Advance(base::TimeDelta::FromHours(1));
  app_list.mutable_apps(0)->set_no_display(true);
  EXPECT_CALL(observer,
              OnRegistryUpdated(service(), testing::ElementsAre(app_id),
                                testing::IsEmpty(), testing::IsEmpty()));
  service()->UpdateApplicationList(app_list);
  result = service()->GetRegistration(app_id);
  EXPECT_EQ(result->InstallTime(), install_time);
  EXPECT_EQ(result->LastLaunchTime(),
            base::Time() + base::TimeDelta::FromHours(3));
}

// Test that UpdateApplicationList doesn't clobber apps from different VMs or
// containers.
TEST_F(CrostiniRegistryServiceTest, MultipleContainers) {
  service()->UpdateApplicationList(
      CrostiniTestHelper::BasicAppList("app", "vm 1", "container 1"));
  service()->UpdateApplicationList(
      CrostiniTestHelper::BasicAppList("app", "vm 1", "container 2"));
  service()->UpdateApplicationList(
      CrostiniTestHelper::BasicAppList("app", "vm 2", "container 1"));
  std::string app_id_1 =
      CrostiniTestHelper::GenerateAppId("app", "vm 1", "container 1");
  std::string app_id_2 =
      CrostiniTestHelper::GenerateAppId("app", "vm 1", "container 2");
  std::string app_id_3 =
      CrostiniTestHelper::GenerateAppId("app", "vm 2", "container 1");

  EXPECT_THAT(service()->GetRegisteredAppIds(),
              testing::UnorderedElementsAre(app_id_1, app_id_2, app_id_3,
                                            kCrostiniTerminalId));

  // Clobber app_id_2
  service()->UpdateApplicationList(
      CrostiniTestHelper::BasicAppList("app 2", "vm 1", "container 2"));
  std::string new_app_id =
      CrostiniTestHelper::GenerateAppId("app 2", "vm 1", "container 2");

  EXPECT_THAT(service()->GetRegisteredAppIds(),
              testing::UnorderedElementsAre(app_id_1, app_id_3, new_app_id,
                                            kCrostiniTerminalId));
}

// Test that ClearApplicationList works, and only removes apps from the
// specified container.
TEST_F(CrostiniRegistryServiceTest, ClearApplicationList) {
  service()->UpdateApplicationList(
      CrostiniTestHelper::BasicAppList("app", "vm 1", "container 1"));
  service()->UpdateApplicationList(
      CrostiniTestHelper::BasicAppList("app", "vm 1", "container 2"));
  ApplicationList app_list =
      CrostiniTestHelper::BasicAppList("app", "vm 2", "container 1");
  *app_list.add_apps() = CrostiniTestHelper::BasicApp("app 2");
  service()->UpdateApplicationList(app_list);
  std::string app_id_1 =
      CrostiniTestHelper::GenerateAppId("app", "vm 1", "container 1");
  std::string app_id_2 =
      CrostiniTestHelper::GenerateAppId("app", "vm 1", "container 2");
  std::string app_id_3 =
      CrostiniTestHelper::GenerateAppId("app", "vm 2", "container 1");
  std::string app_id_4 =
      CrostiniTestHelper::GenerateAppId("app 2", "vm 2", "container 1");

  EXPECT_THAT(service()->GetRegisteredAppIds(),
              testing::UnorderedElementsAre(app_id_1, app_id_2, app_id_3,
                                            app_id_4, kCrostiniTerminalId));

  service()->ClearApplicationList("vm 2", "container 1");

  EXPECT_THAT(
      service()->GetRegisteredAppIds(),
      testing::UnorderedElementsAre(app_id_1, app_id_2, kCrostiniTerminalId));
}

TEST_F(CrostiniRegistryServiceTest, GetCrostiniAppIdNoStartupID) {
  ApplicationList app_list =
      CrostiniTestHelper::BasicAppList("app", "vm", "container");
  *app_list.add_apps() = CrostiniTestHelper::BasicApp("cool.app");
  *app_list.add_apps() = CrostiniTestHelper::BasicApp("super");
  service()->UpdateApplicationList(app_list);

  service()->UpdateApplicationList(
      CrostiniTestHelper::BasicAppList("super", "vm 2", "container"));

  EXPECT_THAT(service()->GetRegisteredAppIds(), testing::SizeIs(5));

  EXPECT_TRUE(service()->GetCrostiniShelfAppId(nullptr, nullptr).empty());

  std::string window_app_id = WindowIdForWMClass("App");
  EXPECT_EQ(service()->GetCrostiniShelfAppId(&window_app_id, nullptr),
            CrostiniTestHelper::GenerateAppId("app", "vm", "container"));

  window_app_id = WindowIdForWMClass("cool.app");
  EXPECT_EQ(service()->GetCrostiniShelfAppId(&window_app_id, nullptr),
            CrostiniTestHelper::GenerateAppId("cool.app", "vm", "container"));

  window_app_id = WindowIdForWMClass("super");
  EXPECT_EQ(service()->GetCrostiniShelfAppId(&window_app_id, nullptr),
            "crostini:" + WindowIdForWMClass("super"));

  window_app_id = "org.chromium.termina.wmclientleader.1234";
  EXPECT_EQ(service()->GetCrostiniShelfAppId(&window_app_id, nullptr),
            "crostini:org.chromium.termina.wmclientleader.1234");

  window_app_id = "org.chromium.termina.xid.654321";
  EXPECT_EQ(service()->GetCrostiniShelfAppId(&window_app_id, nullptr),
            "crostini:org.chromium.termina.xid.654321");

  window_app_id = "cool.app";
  EXPECT_EQ(service()->GetCrostiniShelfAppId(&window_app_id, nullptr),
            CrostiniTestHelper::GenerateAppId("cool.app", "vm", "container"));

  window_app_id = "fancy.app";
  EXPECT_EQ(service()->GetCrostiniShelfAppId(&window_app_id, nullptr),
            "crostini:fancy.app");

  window_app_id = "org.chromium.arc.h";
  EXPECT_EQ(service()->GetCrostiniShelfAppId(&window_app_id, nullptr),
            std::string());
}

TEST_F(CrostiniRegistryServiceTest, GetCrostiniAppIdStartupWMClass) {
  ApplicationList app_list =
      CrostiniTestHelper::BasicAppList("app", "vm", "container");
  app_list.mutable_apps(0)->set_startup_wm_class("app_start");
  *app_list.add_apps() = CrostiniTestHelper::BasicApp("app2");
  *app_list.add_apps() = CrostiniTestHelper::BasicApp("app3");
  app_list.mutable_apps(1)->set_startup_wm_class("app2");
  app_list.mutable_apps(2)->set_startup_wm_class("app2");
  service()->UpdateApplicationList(app_list);

  EXPECT_THAT(service()->GetRegisteredAppIds(), testing::SizeIs(4));

  std::string window_app_id = WindowIdForWMClass("app_start");
  EXPECT_EQ(service()->GetCrostiniShelfAppId(&window_app_id, nullptr),
            CrostiniTestHelper::GenerateAppId("app", "vm", "container"));

  window_app_id = WindowIdForWMClass("app2");
  EXPECT_EQ(service()->GetCrostiniShelfAppId(&window_app_id, nullptr),
            "crostini:" + WindowIdForWMClass("app2"));
}

TEST_F(CrostiniRegistryServiceTest, GetCrostiniAppIdStartupNotify) {
  ApplicationList app_list =
      CrostiniTestHelper::BasicAppList("app", "vm", "container");
  app_list.mutable_apps(0)->set_startup_notify(true);
  *app_list.add_apps() = CrostiniTestHelper::BasicApp("app2");
  service()->UpdateApplicationList(app_list);

  std::string window_app_id = "whatever";
  std::string startup_id = "app";
  EXPECT_EQ(service()->GetCrostiniShelfAppId(&window_app_id, &startup_id),
            CrostiniTestHelper::GenerateAppId("app", "vm", "container"));

  startup_id = "app2";
  EXPECT_EQ(service()->GetCrostiniShelfAppId(&window_app_id, &startup_id),
            "crostini:whatever");

  startup_id = "app";
  EXPECT_EQ(service()->GetCrostiniShelfAppId(nullptr, &startup_id),
            CrostiniTestHelper::GenerateAppId("app", "vm", "container"));
}

TEST_F(CrostiniRegistryServiceTest, GetCrostiniAppIdName) {
  ApplicationList app_list =
      CrostiniTestHelper::BasicAppList("app", "vm", "container");
  *app_list.add_apps() = CrostiniTestHelper::BasicApp("app2", "name2");
  service()->UpdateApplicationList(app_list);

  std::string window_app_id = WindowIdForWMClass("name2");
  EXPECT_EQ(service()->GetCrostiniShelfAppId(&window_app_id, nullptr),
            CrostiniTestHelper::GenerateAppId("app2", "vm", "container"));
}

TEST_F(CrostiniRegistryServiceTest, GetCrostiniAppIdNameSkipNoDisplay) {
  ApplicationList app_list =
      CrostiniTestHelper::BasicAppList("app", "vm", "container");
  *app_list.add_apps() = CrostiniTestHelper::BasicApp("app2", "name2");
  *app_list.add_apps() = CrostiniTestHelper::BasicApp("another_app2", "name2",
                                                      true /* no_display */);
  service()->UpdateApplicationList(app_list);

  std::string window_app_id = WindowIdForWMClass("name2");
  EXPECT_EQ(service()->GetCrostiniShelfAppId(&window_app_id, nullptr),
            CrostiniTestHelper::GenerateAppId("app2", "vm", "container"));
}

TEST_F(CrostiniRegistryServiceTest, IsScaledReturnFalseWhenNotSet) {
  std::string app_id =
      CrostiniTestHelper::GenerateAppId("app", "vm", "container");
  ApplicationList app_list =
      CrostiniTestHelper::BasicAppList("app", "vm", "container");
  service()->UpdateApplicationList(app_list);
  base::Optional<CrostiniRegistryService::Registration> registration =
      service()->GetRegistration(app_id);
  EXPECT_TRUE(registration.has_value());
  EXPECT_FALSE(registration.value().IsScaled());
}

TEST_F(CrostiniRegistryServiceTest, SetScaledWorks) {
  std::string app_id =
      CrostiniTestHelper::GenerateAppId("app", "vm", "container");
  ApplicationList app_list =
      CrostiniTestHelper::BasicAppList("app", "vm", "container");
  service()->UpdateApplicationList(app_list);
  service()->SetAppScaled(app_id, true);
  base::Optional<CrostiniRegistryService::Registration> registration =
      service()->GetRegistration(app_id);
  EXPECT_TRUE(registration.has_value());
  EXPECT_TRUE(registration.value().IsScaled());
  service()->SetAppScaled(app_id, false);
  registration = service()->GetRegistration(app_id);
  EXPECT_TRUE(registration.has_value());
  EXPECT_FALSE(registration.value().IsScaled());
}

}  // namespace crostini
