// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <memory>
#include <string>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/hid/hid_connection_tracker_factory.h"
#include "chrome/browser/hid/hid_system_tray_icon.h"
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

using testing::Pair;
using testing::Return;
using testing::UnorderedElementsAre;

constexpr char kTestProfileName[] = "user@gmail.com";

class MockHidSystemTrayIcon : public HidSystemTrayIcon {
 public:
  MOCK_METHOD(void, StageProfile, (Profile*), (override));
  MOCK_METHOD(void, UnstageProfile, (Profile*, bool), (override));
  MOCK_METHOD(bool, ContainProfile, (Profile*), (override));
  MOCK_METHOD(void, ProfileAdded, (Profile*), (override));
  MOCK_METHOD(void, ProfileRemoved, (Profile*), (override));
  MOCK_METHOD(void, NotifyConnectionCountUpdated, (Profile*), (override));
};

}  // namespace

class HidConnectionTrackerTest : public BrowserWithTestWindowTest {
 public:
  HidConnectionTrackerTest() = default;
  HidConnectionTrackerTest(const HidConnectionTrackerTest&) = delete;
  HidConnectionTrackerTest& operator=(const HidConnectionTrackerTest&) = delete;
  ~HidConnectionTrackerTest() override = default;

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    BrowserList::SetLastActive(browser());

    auto hid_system_tray_icon = std::make_unique<MockHidSystemTrayIcon>();
    hid_system_tray_icon_ = hid_system_tray_icon.get();
    TestingBrowserProcess::GetGlobal()->SetHidSystemTrayIcon(
        std::move(hid_system_tray_icon));

    hid_connection_tracker_ =
        HidConnectionTrackerFactory::GetForProfile(profile(), /*create=*/true);
  }

  void TearDown() override {
    // Set the system tray icon to null to avoid uninteresting call to it during
    // profile destruction.
    TestingBrowserProcess::GetGlobal()->SetHidSystemTrayIcon(nullptr);
    BrowserWithTestWindowTest::TearDown();
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
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
    Profile* profile = profile_manager()->CreateTestingProfile(profile_name);
    return profile;
  }

  void TestDeviceConnection(bool has_system_tray_icon) {
    auto origin1 = url::Origin::Create(GURL("https://www.example1.com"));
    auto origin2 = url::Origin::Create(GURL("https://www.example2.com"));

    // First connection that stages the profile.
    if (has_system_tray_icon) {
      EXPECT_CALL(hid_system_tray_icon(), StageProfile(profile()));
    }
    hid_connection_tracker().IncrementConnectionCount(origin1);
    EXPECT_EQ(hid_connection_tracker().total_connection_count(), 1);
    EXPECT_THAT(hid_connection_tracker().origins(),
                UnorderedElementsAre(Pair(origin1, 1)));

    // Connections from two origins come and go.
    if (has_system_tray_icon) {
      EXPECT_CALL(hid_system_tray_icon(),
                  NotifyConnectionCountUpdated(profile()))
          .Times(4);
    }
    hid_connection_tracker().IncrementConnectionCount(origin1);
    EXPECT_EQ(hid_connection_tracker().total_connection_count(), 2);
    EXPECT_THAT(hid_connection_tracker().origins(),
                UnorderedElementsAre(Pair(origin1, 2)));
    hid_connection_tracker().IncrementConnectionCount(origin2);
    EXPECT_EQ(hid_connection_tracker().total_connection_count(), 3);
    EXPECT_THAT(hid_connection_tracker().origins(),
                UnorderedElementsAre(Pair(origin1, 2), Pair(origin2, 1)));
    hid_connection_tracker().DecrementConnectionCount(origin1);
    EXPECT_EQ(hid_connection_tracker().total_connection_count(), 2);
    EXPECT_THAT(hid_connection_tracker().origins(),
                UnorderedElementsAre(Pair(origin1, 1), Pair(origin2, 1)));
    hid_connection_tracker().DecrementConnectionCount(origin1);
    EXPECT_EQ(hid_connection_tracker().total_connection_count(), 1);
    EXPECT_THAT(hid_connection_tracker().origins(),
                UnorderedElementsAre(Pair(origin2, 1)));

    // The last connection that will unstage the profile.
    if (has_system_tray_icon) {
      EXPECT_CALL(hid_system_tray_icon(),
                  UnstageProfile(profile(), /*immediate=*/false));
    }
    hid_connection_tracker().DecrementConnectionCount(origin2);
    EXPECT_EQ(hid_connection_tracker().total_connection_count(), 0);
    EXPECT_TRUE(hid_connection_tracker().origins().empty());
  }

 private:
  raw_ptr<HidConnectionTracker> hid_connection_tracker_;
  raw_ptr<MockHidSystemTrayIcon> hid_system_tray_icon_;
};

TEST_F(HidConnectionTrackerTest, DeviceConnection) {
  TestDeviceConnection(/*has_system_tray_icon=*/true);
}

// Test the scenario with null HID system tray icon and it doesn't cause crash.
TEST_F(HidConnectionTrackerTest, DeviceConnectionWithNullSystemTrayIcon) {
  TestingBrowserProcess::GetGlobal()->SetHidSystemTrayIcon(nullptr);
  TestDeviceConnection(/*has_system_tray_icon=*/false);
}

TEST_F(HidConnectionTrackerTest, ProfileDestroyed) {
  auto origin = url::Origin::Create(GURL("https://www.example.com"));
  auto* profile_to_be_destroyed = CreateTestingProfile(kTestProfileName);

  auto* connection_tracker =
      HidConnectionTrackerFactory::GetForProfile(profile_to_be_destroyed,
                                                 /*create=*/true);

  EXPECT_CALL(hid_system_tray_icon(), StageProfile(profile_to_be_destroyed));
  connection_tracker->IncrementConnectionCount(origin);
  EXPECT_EQ(connection_tracker->total_connection_count(), 1);
  EXPECT_THAT(connection_tracker->origins(),
              UnorderedElementsAre(Pair(origin, 1)));

  EXPECT_CALL(hid_system_tray_icon(),
              NotifyConnectionCountUpdated(profile_to_be_destroyed));
  connection_tracker->IncrementConnectionCount(origin);
  EXPECT_EQ(connection_tracker->total_connection_count(), 2);
  EXPECT_THAT(connection_tracker->origins(),
              UnorderedElementsAre(Pair(origin, 2)));

  // ContainProfile should be called twice, once from
  // HidConnectionTrackerFactory::BrowserContextShutdown and once from
  // ~HidConnectionTracker.
  EXPECT_CALL(hid_system_tray_icon(), ContainProfile(profile_to_be_destroyed))
      .WillOnce(testing::Return(true))
      .WillOnce(testing::Return(false));
  EXPECT_CALL(hid_system_tray_icon(),
              UnstageProfile(profile_to_be_destroyed, /*immediate=*/true));
  profile_manager()->DeleteTestingProfile(kTestProfileName);
}
