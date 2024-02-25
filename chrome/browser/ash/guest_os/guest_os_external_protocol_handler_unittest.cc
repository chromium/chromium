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

  void AddApp(const std::string& name,
              const std::string& desktop_file_id,
              const std::string& mime_type) {
    vm_tools::apps::App& app = *app_list_.add_apps();
    app.mutable_name()->add_values()->set_value(name);
    app.set_desktop_file_id(desktop_file_id);
    app.add_mime_types(mime_type);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  crostini::FakeCrostiniFeatures fake_crostini_features_;
  vm_tools::apps::ApplicationList app_list_;
};

TEST_F(GuestOsExternalProtocolHandlerTest, TestNoRegisteredApps) {
  AddApp("App", "id", "not-scheme");
  GuestOsRegistryService(profile()).UpdateApplicationList(app_list());

  EXPECT_FALSE(
      GuestOsUrlHandler::GetForUrl(profile(), GURL("testscheme:12341234")));
}

TEST_F(GuestOsExternalProtocolHandlerTest, SingleRegisteredApp) {
  AddApp("App", "id", "x-scheme-handler/testscheme");
  GuestOsRegistryService(profile()).UpdateApplicationList(app_list());

  EXPECT_TRUE(
      GuestOsUrlHandler::GetForUrl(profile(), GURL("testscheme:12341234")));
}

TEST_F(GuestOsExternalProtocolHandlerTest, MostRecent) {
  AddApp("App1", "id1", "x-scheme-handler/testscheme");
  AddApp("App2", "id2", "x-scheme-handler/testscheme");
  GuestOsRegistryService(profile()).UpdateApplicationList(app_list());

  GuestOsRegistryService(profile()).AppLaunched(
      GuestOsRegistryService::GenerateAppId("id1", "vm_name",
                                            "container_name"));

  auto registration =
      GuestOsUrlHandler::GetForUrl(profile(), GURL("testscheme:12341234"));
  ASSERT_TRUE(registration);
  EXPECT_EQ("App1", registration->name());
}

TEST_F(GuestOsExternalProtocolHandlerTest, TransientUrlHandlerIsInvoked) {
  GuestOsRegistryService service(profile());
  int invocations = 0;
  service.RegisterTransientUrlHandler(
      /*handler=*/GuestOsUrlHandler(
          "Handler1", base::BindRepeating(
                          [](int& invocations, Profile*, const GURL& url) {
                            invocations++;
                            EXPECT_EQ(url.spec(), "test://test");
                          },
                          std::ref(invocations))),
      /*canHandleCallback=*/base::BindRepeating(
          [](const GURL& url) { return url.SchemeIs("test"); }));

  GURL url{"test://test"};
  std::optional<GuestOsUrlHandler> handler = service.GetHandler(url);
  ASSERT_TRUE(handler);
  handler->Handle(profile(), url);

  EXPECT_EQ(invocations, 1);
}

TEST_F(GuestOsExternalProtocolHandlerTest,
       InapplicableTransientUrlHandlersIgnored) {
  GuestOsRegistryService service(profile());
  service.RegisterTransientUrlHandler(
      /*handler=*/GuestOsUrlHandler(
          "Handler1", base::BindRepeating([](Profile*, const GURL& url) {})),
      /*canHandleCallback=*/base::BindRepeating(
          [](const GURL& url) { return url.SchemeIs("test"); }));

  std::optional<GuestOsUrlHandler> handler =
      service.GetHandler(GURL("otherscheme://test"));
  EXPECT_FALSE(handler);
}

TEST_F(GuestOsExternalProtocolHandlerTest, OffTheRecordProfile) {
  auto* otr_profile = profile()->GetOffTheRecordProfile(
      Profile::OTRProfileID::CreateUniqueForTesting(),
      /*create_if_needed=*/true);

  EXPECT_FALSE(
      GuestOsUrlHandler::GetForUrl(otr_profile, GURL("testscheme:12341234")));

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
    AddApp("App", "id",
           std::string("x-scheme-handler/") + borealis::kAllowedScheme);
    GuestOsRegistryService(profile()).UpdateApplicationList(app_list());
  }

 private:
  std::unique_ptr<borealis::ScopedAllowBorealis> allow_borealis_;
};

TEST_F(GuestOsExternalProtocolHandlerBorealisTest, AllowedURL) {
  EXPECT_TRUE(
      GuestOsUrlHandler::GetForUrl(profile(), GURL("steam://store/9001")));
  EXPECT_TRUE(GuestOsUrlHandler::GetForUrl(profile(), GURL("steam://run/400")));
}

TEST_F(GuestOsExternalProtocolHandlerBorealisTest, DisallowedURL) {
  // Wrong scheme
  EXPECT_FALSE(GuestOsUrlHandler::GetForUrl(
      profile(), GURL("notborealisscheme://run/12345")));
  // Action not in allow-list
  EXPECT_FALSE(
      GuestOsUrlHandler::GetForUrl(profile(), GURL("steam://uninstall/12345")));
  // Invalid app id
  EXPECT_FALSE(GuestOsUrlHandler::GetForUrl(profile(), GURL("steam://store/")));
  EXPECT_FALSE(GuestOsUrlHandler::GetForUrl(
      profile(),
      GURL("steam://run/"
           "1337133713371337133713371337133713371337133713371337133713371337133"
           "7133713371337133713371337133713371337133713371337133713371337133713"
           "3713371337133713371337133713371337133713371337133713371337133713371"
           "3371337133713371337133713371337133713371337133713371337133713371337"
           "1337133713371337133713371337133713371337133713371337133713371337133"
           "7133713371337133713371337133713371337133713371337133713371337")));
}
}  // namespace guest_os
