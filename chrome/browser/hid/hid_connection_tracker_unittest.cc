// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/hid/hid_connection_tracker_factory.h"
#include "chrome/browser/hid/hid_system_tray_icon.h"
#include "chrome/browser/notifications/notification_display_service_tester.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "base/command_line.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

constexpr char kExtensionName[] = "Fake extension";
constexpr char kTestProfileName[] = "user@gmail.com";

class MockHidConnectionTracker : public HidConnectionTracker {
 public:
  explicit MockHidConnectionTracker(Profile* profile)
      : HidConnectionTracker(profile) {}
  ~MockHidConnectionTracker() override = default;
  MOCK_METHOD(void, ShowSiteSettings, (const url::Origin& origin), (override));
};

BrowserContextKeyedServiceFactory::TestingFactory
GetHidConnectionTrackerTestingFactory() {
  return base::BindRepeating([](content::BrowserContext* browser_context) {
    return static_cast<std::unique_ptr<KeyedService>>(
        std::make_unique<MockHidConnectionTracker>(
            Profile::FromBrowserContext(browser_context)));
  });
}

class MockHidSystemTrayIcon : public HidSystemTrayIcon {
 public:
  MOCK_METHOD(void, AddProfile, (Profile*), (override));
  MOCK_METHOD(void, RemoveProfile, (Profile*), (override));
  MOCK_METHOD(void, NotifyConnectionCountUpdated, (Profile*), (override));
};

class HidConnectionTrackerTest : public BrowserWithTestWindowTest {
 public:
  HidConnectionTrackerTest() = default;
  HidConnectionTrackerTest(const HidConnectionTrackerTest&) = delete;
  HidConnectionTrackerTest& operator=(const HidConnectionTrackerTest&) = delete;
  ~HidConnectionTrackerTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    BrowserList::SetLastActive(browser());

    // TODO(crbug.com/1399310): Pass testing factory when creating profile.
    // Ideally, we should inject MockHidConnectionTracker by overriding
    // BrowserWithTestWindowTest::GetTestingFactories(). However, due to the
    // fact that:
    // 1) TestingProfile::TestingProfile(...) will call BrowserContextShutdown
    //    as part of setting testing factory.
    // 2) HidConnectionTrackerFactory::BrowserContextShutdown() at some point
    //    need valid profile_metrics::GetBrowserProfileType() as part of
    //    HidConnectionTrackerFactory::GetForProfile().
    // It will hit failure in profile_metrics::GetBrowserProfileType() because
    // the profile is not initialized properly before setting testing factory.
    // As a result, here set the testing factory for MockHidConnectionTracker
    // after profile() is properly initialized.
    HidConnectionTrackerFactory::GetInstance()->SetTestingFactory(
        profile(), GetHidConnectionTrackerTestingFactory());

    display_service_ =
        std::make_unique<NotificationDisplayServiceTester>(profile());
    auto hid_system_tray_icon = std::make_unique<MockHidSystemTrayIcon>();
    hid_system_tray_icon_ = hid_system_tray_icon.get();
    TestingBrowserProcess::GetGlobal()->SetHidSystemTrayIcon(
        std::move(hid_system_tray_icon));

    hid_connection_tracker_ = static_cast<MockHidConnectionTracker*>(
        HidConnectionTrackerFactory::GetForProfile(profile(), /*create=*/true));
  }

  std::u16string GetExpectedDeviceConnectedByExtensionNotificationTitle() {
    return u"An extension is using a HID device";
  }

  std::string GetExpectedNotificationId(const url::Origin& origin) {
    return base::StringPrintf("webhid.opened.%s.%s",
                              profile()->UniqueId().c_str(),
                              origin.host().c_str());
  }

  void CheckDeviceConnectedNotification(
      const url::Origin& origin,
      const std::string& name_in_notification_title) {
    auto expected_notification_id = GetExpectedNotificationId(origin);
    EXPECT_EQ(display_service_
                  ->GetDisplayedNotificationsForType(
                      NotificationHandler::Type::TRANSIENT)
                  .size(),
              1u);
    auto maybe_notification =
        display_service_->GetNotification(expected_notification_id);
    ASSERT_TRUE(maybe_notification);
#if BUILDFLAG(ENABLE_EXTENSIONS)
    EXPECT_EQ(maybe_notification->title(),
              GetExpectedDeviceConnectedByExtensionNotificationTitle());
    EXPECT_EQ(maybe_notification->message(),
              GetExpectedDeviceConnectedByExtensionNotificationMessage(
                  name_in_notification_title));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
    EXPECT_TRUE(maybe_notification->delegate());
    EXPECT_CALL(*hid_connection_tracker_, ShowSiteSettings(origin));
    display_service_->SimulateClick(NotificationHandler::Type::TRANSIENT,
                                    expected_notification_id,
                                    /*action_index=*/absl::nullopt,
                                    /*reply=*/absl::nullopt);
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::u16string GetExpectedDeviceConnectedByExtensionNotificationMessage(
      const std::string& name) {
    return base::UTF8ToUTF16(base::StringPrintf(
        "Click to manage permissions for \"%s\"", name.c_str()));
  }

  scoped_refptr<const extensions::Extension> CreateExtensionWithName(
      const std::string& extension_name) {
    extensions::DictionaryBuilder manifest;
    manifest.Set("name", extension_name)
        .Set("description", "For testing.")
        .Set("version", "0.1")
        .Set("manifest_version", 2)
        .Set("web_accessible_resources",
             extensions::ListBuilder().Append("index.html").Build());
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder().SetManifest(manifest.Build()).Build();
    if (!extension) {
      return nullptr;
    }
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile()));
    extensions::ExtensionService* extension_service =
        extension_system->CreateExtensionService(
            base::CommandLine::ForCurrentProcess(), base::FilePath(),
            /*autoupdate_enabled=*/false);
    extension_service->AddExtension(extension.get());
    return extension;
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  HidConnectionTracker& hid_connection_tracker() {
    return *hid_connection_tracker_;
  }

  MockHidSystemTrayIcon& hid_system_tray_icon() {
    return *hid_system_tray_icon_;
  }

  Profile* CreateTestingProfile(const std::string& profile_name) {
    // Ideally, we should be able to pass testing factory when calling profile
    // manager's CreateTestingProfile. However, due to the fact that:
    // 1) TestingProfile::TestingProfile(...) will call BrowserContextShutdown
    //    as part of setting testing factory.
    // 2) HidConnectionTrackerFactory::BrowserContextShutdown() at some point
    //    need valid profile_metrics::GetBrowserProfileType() as part of
    //    HidConnectionTrackerFactory::GetForProfile().
    // It will hit failure in profile_metrics::GetBrowserProfileType() because
    // the profile is not initialized properly before setting testing factory.
    // As a result, here create a profile then call SetTestingFactory to inject
    // MockHidConnectionTracker.
    Profile* profile = profile_manager()->CreateTestingProfile(profile_name);
    HidConnectionTrackerFactory::GetInstance()->SetTestingFactory(
        profile, GetHidConnectionTrackerTestingFactory());
    return profile;
  }

 private:
  std::unique_ptr<NotificationDisplayServiceTester> display_service_;
  raw_ptr<MockHidConnectionTracker> hid_connection_tracker_;
  raw_ptr<MockHidSystemTrayIcon> hid_system_tray_icon_;
};

}  // namespace

TEST_F(HidConnectionTrackerTest, DeviceConnection) {
  EXPECT_CALL(hid_system_tray_icon(), AddProfile(profile()));
  hid_connection_tracker().IncrementConnectionCount();
  EXPECT_CALL(hid_system_tray_icon(), NotifyConnectionCountUpdated(profile()));
  hid_connection_tracker().IncrementConnectionCount();
  EXPECT_CALL(hid_system_tray_icon(), NotifyConnectionCountUpdated(profile()));
  hid_connection_tracker().DecrementConnectionCount();
  EXPECT_CALL(hid_system_tray_icon(), RemoveProfile(profile()));
  hid_connection_tracker().DecrementConnectionCount();
}

TEST_F(HidConnectionTrackerTest, DeviceConnectionWithNullSystemTrayIcon) {
  // Test the scenario with null HID system tray icon and it doesn't cause
  // crash.
  TestingBrowserProcess::GetGlobal()->SetHidSystemTrayIcon(nullptr);
  hid_connection_tracker().IncrementConnectionCount();
  hid_connection_tracker().IncrementConnectionCount();
  hid_connection_tracker().DecrementConnectionCount();
  hid_connection_tracker().DecrementConnectionCount();
}

TEST_F(HidConnectionTrackerTest, ProfileDestroyed) {
  CreateTestingProfile(kTestProfileName);
  EXPECT_CALL(hid_system_tray_icon(), AddProfile(profile()));
  hid_connection_tracker().IncrementConnectionCount();
  EXPECT_CALL(hid_system_tray_icon(), NotifyConnectionCountUpdated(profile()));
  hid_connection_tracker().IncrementConnectionCount();
  EXPECT_CALL(hid_system_tray_icon(), RemoveProfile(profile()));
  profile_manager()->DeleteTestingProfile(kTestProfileName);
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(HidConnectionTrackerTest, DeviceConnectedNotificationByExtension) {
  std::string extension_name(kExtensionName);
  auto extension = CreateExtensionWithName(extension_name);
  hid_connection_tracker().NotifyDeviceConnected(extension->origin());
  CheckDeviceConnectedNotification(extension->origin(), extension_name);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
