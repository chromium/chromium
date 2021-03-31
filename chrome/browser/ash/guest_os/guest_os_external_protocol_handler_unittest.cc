// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_external_protocol_handler.h"

#include <vector>

#include "base/time/time.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/chromeos/crostini/fake_crostini_features.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/vm_applications/apps.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace guest_os {

class GuestOsExternalProtocolHandlerTest : public testing::Test {
  void SetUp() override {
    fake_crostini_features_.set_enabled(true);

    app_list_.set_vm_type(vm_tools::apps::ApplicationList::TERMINA);
    app_list_.set_vm_name("vm_name");
    app_list_.set_container_name("container_name");
  }

 protected:
  TestingProfile* profile() { return &profile_; }
  vm_tools::apps::ApplicationList& app_list() { return app_list_; }

  void AddApp(const std::string& desktop_file_id,
              const std::string& mime_type,
              const base::Time last_launch) {
    vm_tools::apps::App& app = *app_list_.add_apps();
    app.set_desktop_file_id(desktop_file_id);
    app.mutable_name()->add_values();
    app.add_mime_types(mime_type);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  crostini::FakeCrostiniFeatures fake_crostini_features_;
  vm_tools::apps::ApplicationList app_list_;
};

TEST_F(GuestOsExternalProtocolHandlerTest, TestNoRegisteredApps) {
  AddApp("id", "not-scheme", base::Time());
  GuestOsRegistryService(profile()).UpdateApplicationList(app_list());

  EXPECT_FALSE(guest_os::GetHandler(profile(), GURL("testscheme:12341234")));
}

TEST_F(GuestOsExternalProtocolHandlerTest, SingleRegisteredApp) {
  AddApp("id", "x-scheme-handler/testscheme", base::Time());
  GuestOsRegistryService(profile()).UpdateApplicationList(app_list());

  EXPECT_TRUE(guest_os::GetHandler(profile(), GURL("testscheme:12341234")));
}

TEST_F(GuestOsExternalProtocolHandlerTest, MostRecent) {
  AddApp("id1", "x-scheme-handler/testscheme", base::Time::FromTimeT(1));
  AddApp("id2", "x-scheme-handler/testscheme", base::Time::FromTimeT(2));
  GuestOsRegistryService(profile()).UpdateApplicationList(app_list());

  base::Optional<GuestOsRegistryService::Registration> registration =
      GetHandler(profile(), GURL("testscheme:12341234"));
  EXPECT_TRUE(registration);
  EXPECT_EQ("id2", registration->DesktopFileId());
}

TEST_F(GuestOsExternalProtocolHandlerTest, OffTheRecordProfile) {
  auto* otr_profile =
      profile()->GetOffTheRecordProfile(Profile::OTRProfileID("otr-id"));

  EXPECT_FALSE(guest_os::GetHandler(otr_profile, GURL("testscheme:12341234")));

  profile()->DestroyOffTheRecordProfile(otr_profile);
}

}  // namespace guest_os
