// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_shelf_utils.h"

#include <iterator>
#include <memory>

#include "base/types/optional_util.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace guest_os {

namespace {

struct App {
  std::string desktop_file_id;
  std::string vm_name = "vm";
  std::string container_name = "container";
  std::string app_name;
  absl::optional<std::string> startup_wm_class;
  absl::optional<bool> startup_notify;
  absl::optional<bool> no_display;
};

struct WindowIds {
  absl::optional<std::string> app_id;
  absl::optional<std::string> startup_id;
};

std::string GenAppId(const App& app) {
  return crostini::CrostiniTestHelper::GenerateAppId(
      app.desktop_file_id, app.vm_name, app.container_name);
}

}  // namespace

class GuestOsShelfUtilsTest : public testing::Test {
 public:
  std::string GetShelfAppIdUsingProfile(const Profile* profile,
                                        WindowIds window_ids) const {
    return GetGuestShelfAppId(profile, base::OptionalToPtr(window_ids.app_id),
                              base::OptionalToPtr(window_ids.startup_id));
  }

  std::string GetShelfAppId(WindowIds window_ids) const {
    return GetShelfAppIdUsingProfile(&testing_profile_, window_ids);
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

  const Profile* GetOffTheRecordProfile() {
    return testing_profile_.GetOffTheRecordProfile(
        Profile::OTRProfileID::CreateUniqueForTesting(), true);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile testing_profile_;
};

TEST_F(GuestOsShelfUtilsTest,
       GetGuestShelfAppIdReturnsEmptyIdWhenCalledWithoutAnyParametersSet) {
  SetGuestOsRegistry({});

  EXPECT_EQ(GetShelfAppId(WindowIds()), "");
}

TEST_F(GuestOsShelfUtilsTest,
       GetGuestShelfAppIdReturnsEmptyIdForIneligibleProfile) {
  EXPECT_EQ(GetShelfAppIdUsingProfile(GetOffTheRecordProfile(), WindowIds()),
            "");
}

TEST_F(GuestOsShelfUtilsTest,
       GetGuestShelfAppIdFindsAppWithEitherWindowIdOrAppId) {
  SetGuestOsRegistry({
      {.desktop_file_id = "cool.app"},
  });

  // App is found using wm app_id.
  EXPECT_EQ(GetShelfAppId({.app_id = "org.chromium.termina.wmclass.cool.app"}),
            GenAppId({.desktop_file_id = "cool.app"}));

  // App is found using app_id.
  EXPECT_EQ(GetShelfAppId({.app_id = "cool.app"}),
            GenAppId({.desktop_file_id = "cool.app"}));
}

TEST_F(GuestOsShelfUtilsTest, GetGuestShelfAppIdIgnoresWindowAppIdsCase) {
  SetGuestOsRegistry({
      {.desktop_file_id = "app"},
  });

  // App is found using capitalized App.
  EXPECT_EQ(GetShelfAppId({.app_id = "org.chromium.termina.wmclass.App"}),
            GenAppId({.desktop_file_id = "app"}));
}

TEST_F(
    GuestOsShelfUtilsTest,
    GetGuestShelfAppIdCantFindAppWhenMultipleAppsInDifferentVmsShareDesktopFileIds) {
  SetGuestOsRegistry({
      {.desktop_file_id = "super"},
      {.desktop_file_id = "super", .vm_name = "vm 2"},
  });

  // Neither app is found, as they can't be disambiguated.
  EXPECT_EQ(GetShelfAppId({.app_id = "org.chromium.termina.wmclass.super"}),
            "crostini:org.chromium.termina.wmclass.super");
}

TEST_F(GuestOsShelfUtilsTest,
       GetGuestShelfAppIdDoesntFindAppWhenGivenUnregisteredAppIds) {
  SetGuestOsRegistry({});

  EXPECT_EQ(
      GetShelfAppId({.app_id = "org.chromium.termina.wmclientleader.1234"}),
      "crostini:org.chromium.termina.wmclientleader.1234");

  EXPECT_EQ(GetShelfAppId({.app_id = "org.chromium.termina.xid.654321"}),
            "crostini:org.chromium.termina.xid.654321");

  EXPECT_EQ(GetShelfAppId({.app_id = "fancy.app"}), "crostini:fancy.app");
}

TEST_F(GuestOsShelfUtilsTest,
       GetGuestShelfAppIdCanFindAppUsingItsStartupWmClass) {
  SetGuestOsRegistry({
      {.desktop_file_id = "app", .startup_wm_class = "app_start"},
  });

  // App is found using it's startup_wm_class.
  EXPECT_EQ(GetShelfAppId({.app_id = "org.chromium.termina.wmclass.app_start"}),
            GenAppId({.desktop_file_id = "app"}));
}

TEST_F(GuestOsShelfUtilsTest,
       GetGuestShelfAppIdCantFindAppIfMultipleAppsStartupWmClassesAreTheSame) {
  SetGuestOsRegistry({
      {.desktop_file_id = "app2", .startup_wm_class = "app2"},
      {.desktop_file_id = "app3", .startup_wm_class = "app2"},
  });

  // Neither app is found, as they can't be disambiguated.
  EXPECT_EQ(GetShelfAppId({.app_id = "org.chromium.termina.wmclass.app2"}),
            "crostini:org.chromium.termina.wmclass.app2");
}

TEST_F(GuestOsShelfUtilsTest,
       GetGuestShelfAppIdCanFindAppUsingStartupIdIfStartupNotifyIsTrue) {
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

TEST_F(GuestOsShelfUtilsTest, GetGuestShelfAppIdCanFindAppsByName) {
  SetGuestOsRegistry({
      {.desktop_file_id = "app", .app_name = "name"},
  });

  // App found by app_name: "name".
  EXPECT_EQ(GetShelfAppId({.app_id = "org.chromium.termina.wmclass.name"}),
            GenAppId({.desktop_file_id = "app"}));
}

TEST_F(GuestOsShelfUtilsTest,
       GetGuestShelfAppIdDoesntFindAppsByNameIfTheyHaveNoDisplaySet) {
  // One no_display app.
  SetGuestOsRegistry({
      {.desktop_file_id = "another_app",
       .app_name = "name",
       .no_display = true},
  });

  // No app is found.
  EXPECT_EQ(GetShelfAppId({.app_id = "org.chromium.termina.wmclass.name"}),
            "crostini:org.chromium.termina.wmclass.name");

  // Two apps with the same name, where one is no_display.
  SetGuestOsRegistry({
      {.desktop_file_id = "app", .app_name = "name"},
      {.desktop_file_id = "another_app",
       .app_name = "name",
       .no_display = true},
  });

  // The app without no_display set is found.
  EXPECT_EQ(GetShelfAppId({.app_id = "org.chromium.termina.wmclass.name"}),
            GenAppId({.desktop_file_id = "app"}));
}

}  // namespace guest_os
