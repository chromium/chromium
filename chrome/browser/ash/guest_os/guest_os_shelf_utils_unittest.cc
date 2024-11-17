// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_shelf_utils.h"

#include <iterator>
#include <memory>
#include <optional>

#include "base/types/optional_util.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker.h"
#include "chrome/browser/ash/guest_os/guest_os_session_tracker_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace guest_os {

namespace {

constexpr char container_token[] = "test_container_token";
constexpr char container_name[] = "container";

struct App {
  std::string desktop_file_id;
  std::string vm_name = crostini::kCrostiniDefaultVmName;
  std::string container_name = "container";
  std::string app_name;
  std::optional<std::string> startup_wm_class;
  std::optional<bool> startup_notify;
  std::optional<bool> no_display;
};

struct WindowIds {
  std::optional<std::string> app_id;
  std::optional<std::string> startup_id;
};

std::string GenAppId(const App& app) {
  return crostini::CrostiniTestHelper::GenerateAppId(
      app.desktop_file_id, app.vm_name, app.container_name);
}

std::string TestWaylandWindowIdWithToken(
    const std::string& app_suffix,
    const std::string& token = container_token) {
  return "org.chromium.guest_os." + token + ".wayland." + app_suffix;
}

std::string TestXWindowIdWithToken(const std::string& app_suffix,
                                   const std::string& token = container_token) {
  return "org.chromium.guest_os." + token + ".wmclass." + app_suffix;
}
}  // namespace

class GuestOsShelfUtilsTest : public testing::Test {
 public:
  std::string GetGuestOsShelfAppIdUsingProfile(Profile* profile,
                                               WindowIds window_ids) const {
    return GetGuestOsShelfAppId(profile, base::OptionalToPtr(window_ids.app_id),
                                base::OptionalToPtr(window_ids.startup_id));
  }

  std::string GetShelfAppId(WindowIds window_ids) {
    return GetGuestOsShelfAppIdUsingProfile(&testing_profile_, window_ids);
  }

  void SetGuestOsRegistry(std::vector<App> apps) {
    using AppLists = std::map<std::pair<std::string, std::string>,
                              vm_tools::apps::ApplicationList>;
    AppLists app_lists;
    for (App& in_app : apps) {
      AppLists::key_type key(std::move(in_app.vm_name),
                             std::move(in_app.container_name));
      AppLists::iterator app_list_it = app_lists.find(key);
      if (app_list_it == app_lists.cend()) {
        vm_tools::apps::ApplicationList app_list;
        app_list.set_vm_name(key.first);
        app_list.set_container_name(key.second);
        app_list_it = app_lists.emplace_hint(app_list_it, std::move(key),
                                             std::move(app_list));
      }
      vm_tools::apps::App& out_app = *app_list_it->second.add_apps();
      out_app.set_desktop_file_id(std::move(in_app.desktop_file_id));
      out_app.mutable_name()->add_values()->set_value(
          std::move(in_app.app_name));
      if (in_app.startup_wm_class)
        out_app.set_startup_wm_class(std::move(*in_app.startup_wm_class));
      if (in_app.startup_notify)
        out_app.set_startup_notify(*in_app.startup_notify);
      if (in_app.no_display)
        out_app.set_no_display(*in_app.no_display);
    }
    guest_os::GuestOsRegistryService service(&testing_profile_);
    for (AppLists::value_type& value : app_lists) {
      service.UpdateApplicationList(std::move(value.second));
    }
  }

  Profile* GetOffTheRecordProfile() {
    return testing_profile_.GetOffTheRecordProfile(
        Profile::OTRProfileID::CreateUniqueForTesting(), true);
  }

  void SetUp() override {
    guest_os::GuestOsSessionTrackerFactory::GetForProfile(&testing_profile_)
        ->AddGuestForTesting(
            guest_os::GuestId{guest_os::VmType::TERMINA,
                              crostini::kCrostiniDefaultVmName, container_name},
            container_token);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
};

TEST_F(GuestOsShelfUtilsTest,
       GetGuestOsShelfAppIdReturnsEmptyIdWhenCalledWithoutAnyParametersSet) {
  SetGuestOsRegistry({});

  EXPECT_EQ(GetShelfAppId(WindowIds()), "");
}

TEST_F(GuestOsShelfUtilsTest,
       GetGuestOsShelfAppIdReturnsEmptyIdForIneligibleProfile) {
  EXPECT_EQ(
      GetGuestOsShelfAppIdUsingProfile(GetOffTheRecordProfile(), WindowIds()),
      "");
}

TEST_F(GuestOsShelfUtilsTest,
       GetGuestOsShelfAppIdFindsAppWithEitherWindowIdOrAppId) {
  SetGuestOsRegistry({
      {.desktop_file_id = "cool.app"},
  });

  // App is found using wm app_id.
  EXPECT_EQ(GetShelfAppId({.app_id = TestXWindowIdWithToken("cool.app")}),
            GenAppId({.desktop_file_id = "cool.app"}));

  // App is found using app_id (For native Wayland apps).
  EXPECT_EQ(GetShelfAppId({.app_id = TestWaylandWindowIdWithToken("cool.app")}),
            GenAppId({.desktop_file_id = "cool.app"}));
}

TEST_F(GuestOsShelfUtilsTest,
       GetGuestOsShelfAppIdFindsAppForDefaultContainerToken) {
  SetGuestOsRegistry({
      {.desktop_file_id = "cool.app"},
  });

  // App is found using wm app_id.
  EXPECT_EQ(
      GetShelfAppId({.app_id = TestXWindowIdWithToken("cool.app", "termina")}),
      GenAppId({.desktop_file_id = "cool.app"}));

  // App is found using app_id (For native Wayland apps).
  EXPECT_EQ(GetShelfAppId({.app_id = TestWaylandWindowIdWithToken("cool.app",
                                                                  "termina")}),
            GenAppId({.desktop_file_id = "cool.app"}));
}

TEST_F(GuestOsShelfUtilsTest,
       GetGuestOsShelfAppIdCantFindAppWhenTokenIsInvalid) {
  EXPECT_EQ(GetShelfAppId({.app_id = TestXWindowIdWithToken("cool.app",
                                                            "invalid_token")}),
            "crostini:" + TestXWindowIdWithToken("cool.app", "invalid_token"));
}

TEST_F(GuestOsShelfUtilsTest, GetGuestOsShelfAppIdIgnoresWindowAppIdsCase) {
  SetGuestOsRegistry({
      {.desktop_file_id = "app"},
  });

  // App is found using capitalized App.
  EXPECT_EQ(GetShelfAppId({.app_id = TestXWindowIdWithToken("App")}),
            GenAppId({.desktop_file_id = "app"}));
}

TEST_F(
    GuestOsShelfUtilsTest,
    GetGuestOsShelfAppIdFindsAppWhenMultipleAppsInDifferentVmsShareDesktopFileIds) {
  SetGuestOsRegistry({
      {.desktop_file_id = "duplicate"},
      {.desktop_file_id = "duplicate", .vm_name = "vm 2"},
  });

  EXPECT_EQ(GetShelfAppId({.app_id = TestXWindowIdWithToken("duplicate")}),
            GenAppId({.desktop_file_id = "duplicate"}));
}

TEST_F(
    GuestOsShelfUtilsTest,
    GetGuestOsShelfAppIdFindsAppWhenMultipleAppsInDifferentContainersShareDesktopFileIds) {
  SetGuestOsRegistry({
      {.desktop_file_id = "duplicate"},
      {.desktop_file_id = "duplicate", .container_name = "container 2"},
  });

  EXPECT_EQ(GetShelfAppId({.app_id = TestXWindowIdWithToken("duplicate")}),
            GenAppId({.desktop_file_id = "duplicate"}));
}

TEST_F(GuestOsShelfUtilsTest,
       GetGuestOsShelfAppIdDoesntFindAppWhenGivenUnregisteredAppIds) {
  SetGuestOsRegistry({});

  EXPECT_EQ(GetShelfAppId({.app_id = "org.chromium.guest_os.test_container_"
                                     "token.wmclientleader.1234"}),
            "crostini:org.chromium.guest_os.test_container_token."
            "wmclientleader.1234");

  EXPECT_EQ(
      GetShelfAppId(
          {.app_id = "org.chromium.guest_os.test_container_token.xid.654321"}),
      "crostini:org.chromium.guest_os.test_container_token.xid.654321");

  EXPECT_EQ(
      GetShelfAppId(
          {.app_id =
               "org.chromium.guest_os.test_container_token.wayland.fancy.app"}),
      "crostini:org.chromium.guest_os.test_container_token.wayland.fancy.app");
}

TEST_F(GuestOsShelfUtilsTest,
       GetGuestOsShelfAppIdPrependsCorrectGenericPrefixForUnregisteredAppIds) {
  SetGuestOsRegistry({});

  EXPECT_EQ(
      GetShelfAppId({.app_id = "org.chromium.guest_os.termina.wmclass.1"}),
      "crostini:org.chromium.guest_os.termina.wmclass.1");

  EXPECT_EQ(GetShelfAppId({.app_id = "unregistered_app"}),
            "crostini:unregistered_app");

  EXPECT_EQ(GetShelfAppId({.app_id = "org.chromium.guest_os.borealis"
                                     ".xid.1"}),
            "borealis_anon:org.chromium.guest_os.borealis.xid.1");
}

TEST_F(GuestOsShelfUtilsTest,
       GetGuestOsShelfAppIdCanFindAppUsingItsStartupWmClass) {
  SetGuestOsRegistry({
      {.desktop_file_id = "app", .startup_wm_class = "app_start"},
  });

  // App is found using it's startup_wm_class.
  EXPECT_EQ(
      GetShelfAppId(
          {.app_id =
               "org.chromium.guest_os.test_container_token.wmclass.app_start"}),
      GenAppId({.desktop_file_id = "app"}));
}

TEST_F(
    GuestOsShelfUtilsTest,
    GetGuestOsShelfAppIdCantFindAppIfMultipleAppsStartupWmClassesAreTheSame) {
  SetGuestOsRegistry({
      {.desktop_file_id = "app2", .startup_wm_class = "app2"},
      {.desktop_file_id = "app3", .startup_wm_class = "app2"},
  });

  // Neither app is found, as they can't be disambiguated.
  EXPECT_EQ(
      GetShelfAppId(
          {.app_id =
               "org.chromium.guest_os.test_container_token.wmclass.app2"}),
      "crostini:org.chromium.guest_os.test_container_token.wmclass.app2");
}

TEST_F(GuestOsShelfUtilsTest,
       GetGuestOsShelfAppIdCanFindAppUsingStartupIdIfStartupNotifyIsTrue) {
  SetGuestOsRegistry({
      {.desktop_file_id = "app", .startup_notify = true},
      {.desktop_file_id = "app2", .startup_notify = false},
  });

  // App's startup_notify is true, so it can be found using startup_id.
  EXPECT_EQ(GetShelfAppId({.startup_id = "app"}),
            GenAppId({.desktop_file_id = "app"}));

  // App's startup_notify is true, so it can be found using startup_id, even if
  // app_id is set.
  EXPECT_EQ(GetShelfAppId({.app_id = "unknown_app_id", .startup_id = "app"}),
            GenAppId({.desktop_file_id = "app"}));

  // App's startup_notify is false, so startup_id is ignored and no app was
  // found.
  EXPECT_EQ(GetShelfAppId({.app_id = "unknown_app_id", .startup_id = "app2"}),
            "crostini:unknown_app_id");
}

TEST_F(
    GuestOsShelfUtilsTest,
    GetGuestOsShelfAppIdFindsAppUsingStartupIdsIfMultipleAppsInDifferentContainersShareDesktopFileIds) {
  SetGuestOsRegistry({
      {.desktop_file_id = "duplicate", .startup_notify = true},
      {.desktop_file_id = "duplicate",
       .container_name = "container 2",
       .startup_notify = true},
  });

  EXPECT_EQ(GetShelfAppId({.app_id = TestXWindowIdWithToken("duplicate"),
                           .startup_id = "duplicate"}),
            GenAppId({.desktop_file_id = "duplicate"}));
}

TEST_F(GuestOsShelfUtilsTest, GetGuestOsShelfAppIdCanFindAppsByName) {
  SetGuestOsRegistry({
      {.desktop_file_id = "app", .app_name = "name"},
  });

  // App found by app_name: "name".
  EXPECT_EQ(
      GetShelfAppId(
          {.app_id =
               "org.chromium.guest_os.test_container_token.wmclass.name"}),
      GenAppId({.desktop_file_id = "app"}));
}

TEST_F(GuestOsShelfUtilsTest,
       GetGuestOsShelfAppIdDoesntFindAppsByNameIfTheyHaveNoDisplaySet) {
  // One no_display app.
  SetGuestOsRegistry({
      {.desktop_file_id = "another_app",
       .app_name = "name",
       .no_display = true},
  });

  // No app is found.
  EXPECT_EQ(
      GetShelfAppId(
          {.app_id =
               "org.chromium.guest_os.test_container_token.wmclass.name"}),
      "crostini:org.chromium.guest_os.test_container_token.wmclass.name");

  // Two apps with the same name, where one is no_display.
  SetGuestOsRegistry({
      {.desktop_file_id = "app", .app_name = "name"},
      {.desktop_file_id = "another_app",
       .app_name = "name",
       .no_display = true},
  });

  // The app without no_display set is found.
  EXPECT_EQ(
      GetShelfAppId(
          {.app_id =
               "org.chromium.guest_os.test_container_token.wmclass.name"}),
      GenAppId({.desktop_file_id = "app"}));
}

}  // namespace guest_os
