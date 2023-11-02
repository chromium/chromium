// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/guest_os_external_protocol_handler.h"

#include <vector>

#include "base/base64.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/testing/features.h"
#include "chrome/browser/ash/crostini/fake_crostini_features.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/dbus/vm_applications/apps.pb.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace guest_os {

class GuestOsExternalProtocolHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    fake_crostini_features_.set_enabled(true);

    app_list_.set_vm_type(vm_tools::apps::VmType::TERMINA);
    app_list_.set_vm_name("vm_name");
    app_list_.set_container_name("container_name");
  }

  TestingProfile* profile() { return &profile_; }
  vm_tools::apps::ApplicationList& app_list() { return app_list_; }

  void AddApp(const std::string& desktop_file_id,
              const std::string& mime_type) {
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
  AddApp("id", "not-scheme");
  GuestOsRegistryService(profile()).UpdateApplicationList(app_list());

  EXPECT_FALSE(guest_os::GetHandler(profile(), GURL("testscheme:12341234")));
}

TEST_F(GuestOsExternalProtocolHandlerTest, SingleRegisteredApp) {
  AddApp("id", "x-scheme-handler/testscheme");
  GuestOsRegistryService(profile()).UpdateApplicationList(app_list());

  EXPECT_TRUE(guest_os::GetHandler(profile(), GURL("testscheme:12341234")));
}

TEST_F(GuestOsExternalProtocolHandlerTest, MostRecent) {
  AddApp("id1", "x-scheme-handler/testscheme");
  AddApp("id2", "x-scheme-handler/testscheme");
  GuestOsRegistryService(profile()).UpdateApplicationList(app_list());

  GuestOsRegistryService(profile()).AppLaunched(
      GuestOsRegistryService::GenerateAppId("id1", "vm_name",
                                            "container_name"));

  absl::optional<GuestOsRegistryService::Registration> registration =
      GetHandler(profile(), GURL("testscheme:12341234"));
  EXPECT_TRUE(registration);
  EXPECT_EQ("id1", registration->DesktopFileId());
}

TEST_F(GuestOsExternalProtocolHandlerTest, OffTheRecordProfile) {
  auto* otr_profile = profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);

  EXPECT_FALSE(guest_os::GetHandler(otr_profile, GURL("testscheme:12341234")));

  profile()->DestroyOffTheRecordProfile(otr_profile);
}

class GuestOsExternalProtocolHandlerBorealisTest
    : public GuestOsExternalProtocolHandlerTest {
  void SetUp() override {
    GuestOsExternalProtocolHandlerTest::SetUp();

    allow_borealis_ = std::make_unique<borealis::ScopedAllowBorealis>(
        profile(), /*also_enable=*/true);
    SetupBorealisApp();
  }

 protected:
  void SetupBorealisApp() {
    app_list().set_vm_type(vm_tools::apps::VmType::BOREALIS);
    AddApp("id", std::string("x-scheme-handler/") + borealis::kAllowedScheme);
    GuestOsRegistryService(profile()).UpdateApplicationList(app_list());
  }

 private:
  std::unique_ptr<borealis::ScopedAllowBorealis> allow_borealis_;
};

TEST_F(GuestOsExternalProtocolHandlerBorealisTest, AllowedURL) {
  EXPECT_TRUE(guest_os::GetHandler(
      profile(),
      GURL(borealis::kAllowedScheme + std::string(":") +
           std::string(borealis::kURLAllowlist[0]) + std::string("9001"))));
}

TEST_F(GuestOsExternalProtocolHandlerBorealisTest, DisallowedURL) {
  EXPECT_FALSE(guest_os::GetHandler(
      profile(), GURL(std::string("notborealisscheme:") +
                      std::string(borealis::kURLAllowlist[0]))));
  EXPECT_FALSE(guest_os::GetHandler(
      profile(),
      GURL(borealis::kAllowedScheme + std::string(":notborealis/url"))));
}
}  // namespace guest_os
