// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/chrome_ash_message_center_client.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/notifier_metadata.h"
#include "ash/public/cpp/notifier_settings_observer.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/test/permission_test_util.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_result.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/public/cpp/notifier_id.h"

namespace {

class ChromeAshMessageCenterClientTest : public testing::Test,
                                         public ash::NotifierSettingsObserver {
 public:
  ChromeAshMessageCenterClientTest(const ChromeAshMessageCenterClientTest&) =
      delete;
  ChromeAshMessageCenterClientTest& operator=(
      const ChromeAshMessageCenterClientTest&) = delete;

 protected:
  ChromeAshMessageCenterClientTest()
      : testing_profile_manager_(TestingBrowserProcess::GetGlobal()) {}
  ~ChromeAshMessageCenterClientTest() override {}

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(testing_profile_manager_.SetUp());

    // Initialize the UserManager singleton to a fresh FakeUserManager instance.
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<ash::FakeChromeUserManager>());

    message_center::MessageCenter::Initialize();
  }

  void TearDown() override {
    client_.reset();
    message_center::MessageCenter::Shutdown();
  }

  // ash::NotifierSettingsObserver:
  void OnNotifiersUpdated(
      const std::vector<ash::NotifierMetadata>& notifiers) override {
    notifiers_ = notifiers;
  }

  TestingProfile* CreateProfile(const std::string& name) {
    TestingProfile* profile =
        testing_profile_manager_.CreateTestingProfile(name);

    GetFakeUserManager()->AddUser(AccountId::FromUserEmail(name));
    GetFakeUserManager()->LoginUser(AccountId::FromUserEmail(name));
    return profile;
  }

  base::FilePath GetProfilePath(const std::string& base_name) {
    return testing_profile_manager_.profile_manager()
        ->user_data_dir()
        .AppendASCII(base_name);
  }

  void SwitchActiveUser(const std::string& name) {
    GetFakeUserManager()->SwitchActiveUser(AccountId::FromUserEmail(name));
  }

  void CreateClient() {
    client_ = std::make_unique<ChromeAshMessageCenterClient>(nullptr);
    client_->AddNotifierSettingsObserver(this);
  }

  ChromeAshMessageCenterClient* message_center_client() {
    return client_.get();
  }

 protected:
  void RefreshNotifierList() { message_center_client()->GetNotifiers(); }

  std::vector<ash::NotifierMetadata> notifiers_;

 private:
  ash::FakeChromeUserManager* GetFakeUserManager() {
    return static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());
  }

  content::BrowserTaskEnvironment task_environment_;
  TestingProfileManager testing_profile_manager_;
  std::unique_ptr<ChromeAshMessageCenterClient> client_;
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
};

// TODO(mukai): write a test case to reproduce the actual guest session scenario
// in ChromeOS.

TEST_F(ChromeAshMessageCenterClientTest, NotifierSortOrder) {
  TestingProfile* profile = CreateProfile("profile1@gmail.com");
  extensions::TestExtensionSystem* test_extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile));
  base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
  extensions::ExtensionService* extension_service =
      test_extension_system->CreateExtensionService(
          &command_line, base::FilePath() /* install_directory */,
          false /* autoupdate_enabled*/);

  extensions::ExtensionBuilder foo_app;
  // Foo is an app with name Foo and should appear second.
  const std::string kFooId = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

  // Bar is an app with name Bar and should appear first.
  const std::string kBarId = "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

  // Baz is an app with name Baz and should not appear in the notifier list
  // since it doesn't have notifications permission.
  const std::string kBazId = "cccccccccccccccccccccccccccccccc";

  // Baf is a hosted app which should not appear in the notifier list.
  const std::string kBafId = "dddddddddddddddddddddddddddddddd";

  foo_app.SetManifest(
      base::Value::Dict()
          .Set("name", "Foo")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("app",
               base::Value::Dict().Set(
                   "background",
                   base::Value::Dict().Set(
                       "scripts", base::Value::List().Append("background.js"))))
          .Set("permissions", base::Value::List().Append("notifications")));
  foo_app.SetID(kFooId);
  extension_service->AddExtension(foo_app.Build().get());

  extensions::ExtensionBuilder bar_app;
  bar_app.SetManifest(
      base::Value::Dict()
          .Set("name", "Bar")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("app",
               base::Value::Dict().Set(
                   "background",
                   base::Value::Dict().Set(
                       "scripts", base::Value::List().Append("background.js"))))
          .Set("permissions", base::Value::List().Append("notifications")));
  bar_app.SetID(kBarId);
  extension_service->AddExtension(bar_app.Build().get());

  extensions::ExtensionBuilder baz_app;
  baz_app.SetManifest(
      base::Value::Dict()
          .Set("name", "baz")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("app", base::Value::Dict().Set(
                          "background",
                          base::Value::Dict().Set(
                              "scripts",
                              base::Value::List().Append("background.js")))));
  baz_app.SetID(kBazId);
  extension_service->AddExtension(baz_app.Build().get());

  extensions::ExtensionBuilder baf_app;
  baf_app.SetManifest(
      base::Value::Dict()
          .Set("name", "baf")
          .Set("version", "1.0.0")
          .Set("manifest_version", 2)
          .Set("app", base::Value::Dict().Set("urls",
                                              base::Value::List().Append(
                                                  "http://localhost/extensions/"
                                                  "hosted_app/main.html")))
          .Set("launch", base::Value::Dict().Set(
                             "urls", base::Value::List().Append(
                                         "http://localhost/extensions/"
                                         "hosted_app/main.html"))));

  baf_app.SetID(kBafId);
  extension_service->AddExtension(baf_app.Build().get());
  CreateClient();

  RefreshNotifierList();
  ASSERT_EQ(2u, notifiers_.size());
  EXPECT_EQ(kBarId, notifiers_[0].notifier_id.id);
  EXPECT_EQ(kFooId, notifiers_[1].notifier_id.id);
}

TEST_F(ChromeAshMessageCenterClientTest, SetWebPageNotifierEnabled) {
  TestingProfile* profile = CreateProfile("myprofile@gmail.com");
  CreateClient();

  GURL origin("https://example.com/");

  message_center::NotifierId notifier_id(origin);

  ContentSetting default_setting =
      HostContentSettingsMapFactory::GetForProfile(profile)
          ->GetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS,
                                     nullptr);
  ASSERT_EQ(CONTENT_SETTING_ASK, default_setting);

  profile->SetPermissionControllerDelegate(
      permissions::GetPermissionControllerDelegate(profile));

  // (1) Enable the permission when the default is to ask (expected to set).
  message_center_client()->SetNotifierEnabled(notifier_id, true);
  EXPECT_EQ(
      blink::mojom::PermissionStatus::GRANTED,
      profile->GetPermissionController()
          ->GetPermissionResultForOriginWithoutContext(
              blink::PermissionType::NOTIFICATIONS, url::Origin::Create(origin))
          .status);

  // (2) Disable the permission when the default is to ask (expected to clear).
  message_center_client()->SetNotifierEnabled(notifier_id, false);
  EXPECT_EQ(
      blink::mojom::PermissionStatus::ASK,
      profile->GetPermissionController()
          ->GetPermissionResultForOriginWithoutContext(
              blink::PermissionType::NOTIFICATIONS, url::Origin::Create(origin))
          .status);

  // Change the default content setting vaule for notifications to ALLOW.
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS,
                                 CONTENT_SETTING_ALLOW);

  // (3) Disable the permission when the default is allowed (expected to set).
  message_center_client()->SetNotifierEnabled(notifier_id, false);
  EXPECT_EQ(
      blink::mojom::PermissionStatus::DENIED,
      profile->GetPermissionController()
          ->GetPermissionResultForOriginWithoutContext(
              blink::PermissionType::NOTIFICATIONS, url::Origin::Create(origin))
          .status);

  // (4) Enable the permission when the default is allowed (expected to clear).
  message_center_client()->SetNotifierEnabled(notifier_id, true);

  EXPECT_EQ(
      blink::mojom::PermissionStatus::GRANTED,
      profile->GetPermissionController()
          ->GetPermissionResultForOriginWithoutContext(
              blink::PermissionType::NOTIFICATIONS, url::Origin::Create(origin))
          .status);

  // Now change the default content setting value to BLOCK.
  HostContentSettingsMapFactory::GetForProfile(profile)
      ->SetDefaultContentSetting(ContentSettingsType::NOTIFICATIONS,
                                 CONTENT_SETTING_BLOCK);

  // (5) Enable the permission when the default is blocked (expected to set).
  message_center_client()->SetNotifierEnabled(notifier_id, true);
  EXPECT_EQ(
      blink::mojom::PermissionStatus::GRANTED,
      profile->GetPermissionController()
          ->GetPermissionResultForOriginWithoutContext(
              blink::PermissionType::NOTIFICATIONS, url::Origin::Create(origin))
          .status);

  // (6) Disable the permission when the default is blocked (expected to clear).
  message_center_client()->SetNotifierEnabled(notifier_id, false);
  EXPECT_EQ(
      blink::mojom::PermissionStatus::DENIED,
      profile->GetPermissionController()
          ->GetPermissionResultForOriginWithoutContext(
              blink::PermissionType::NOTIFICATIONS, url::Origin::Create(origin))
          .status);
}

}  // namespace
