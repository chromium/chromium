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
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace {

using base::TimeTicks;
using testing::Pair;
using testing::Return;
using testing::UnorderedElementsAre;

using OriginState = HidConnectionTracker::OriginState;

constexpr char kTestProfileName[] = "user@gmail.com";

class MockHidSystemTrayIcon : public HidSystemTrayIcon {
 public:
  MOCK_METHOD(void, StageProfile, (Profile*), (override));
  MOCK_METHOD(void, UnstageProfile, (Profile*, bool), (override));
  MOCK_METHOD(void, ProfileAdded, (Profile*), (override));
  MOCK_METHOD(void, ProfileRemoved, (Profile*), (override));
  MOCK_METHOD(void, NotifyConnectionCountUpdated, (Profile*), (override));
};

}  // namespace

class HidConnectionTrackerTest : public BrowserWithTestWindowTest {
 public:
  HidConnectionTrackerTest()
      : BrowserWithTestWindowTest(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
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
    auto manifest = base::Value::Dict()
                        .Set("name", extension_name)
                        .Set("description", "For testing.")
                        .Set("version", "0.1")
                        .Set("manifest_version", 2)
                        .Set("web_accessible_resources",
                             base::Value::List().Append("index.html"));
    scoped_refptr<const extensions::Extension> extension =
        extensions::ExtensionBuilder(/*name=*/extension_name)
            .MergeManifest(std::move(manifest))
            .Build();
    CHECK(extension);
    return extension;
  }

  void AddExtensionToProfile(Profile* profile,
                             const extensions::Extension* extension) {
    extensions::TestExtensionSystem* extension_system =
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile));
    extensions::ExtensionService* extension_service =
        extension_system->extension_service();
    if (!extension_service) {
      extension_service = extension_system->CreateExtensionService(
          base::CommandLine::ForCurrentProcess(), base::FilePath(),
          /*autoupdate_enabled=*/false);
    }
    extension_service->AddExtension(extension);
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

  void TestDeviceConnection(
      bool has_system_tray_icon,
      const std::vector<std::pair<url::Origin, std::string>>&
          origin_name_pairs) {
    ASSERT_EQ(origin_name_pairs.size(), 2u);
    auto t0 = TimeTicks::Now();

    // First connection of the first origin stages the profile.
    if (has_system_tray_icon) {
      EXPECT_CALL(hid_system_tray_icon(), StageProfile(profile()));
    }
    hid_connection_tracker().IncrementConnectionCount(
        origin_name_pairs[0].first);
    EXPECT_EQ(hid_connection_tracker().total_connection_count(), 1);
    EXPECT_THAT(hid_connection_tracker().origins(),
                UnorderedElementsAre(
                    Pair(origin_name_pairs[0].first,
                         OriginState(1, t0, origin_name_pairs[0].second))));
    testing::Mock::VerifyAndClearExpectations(&hid_connection_tracker());

    // The second origin comes in at t1.
    task_environment()->FastForwardBy(base::Seconds(1));
    auto t1 = TimeTicks::Now();
    if (has_system_tray_icon) {
      EXPECT_CALL(hid_system_tray_icon(),
                  NotifyConnectionCountUpdated(profile()))
          .Times(2);
    }
    hid_connection_tracker().IncrementConnectionCount(
        origin_name_pairs[1].first);
    EXPECT_EQ(hid_connection_tracker().total_connection_count(), 2);
    EXPECT_THAT(hid_connection_tracker().origins(),
                UnorderedElementsAre(
                    Pair(origin_name_pairs[0].first,
                         OriginState(1, t0, origin_name_pairs[0].second)),
                    Pair(origin_name_pairs[1].first,
                         OriginState(1, t1, origin_name_pairs[1].second))));
    hid_connection_tracker().IncrementConnectionCount(
        origin_name_pairs[0].first);
    EXPECT_EQ(hid_connection_tracker().total_connection_count(), 3);
    EXPECT_THAT(hid_connection_tracker().origins(),
                UnorderedElementsAre(
                    Pair(origin_name_pairs[0].first,
                         OriginState(2, t1, origin_name_pairs[0].second)),
                    Pair(origin_name_pairs[1].first,
                         OriginState(1, t1, origin_name_pairs[1].second))));
    testing::Mock::VerifyAndClearExpectations(&hid_connection_tracker());

    // Two origins are removed 5 seconds apart.
    if (has_system_tray_icon) {
      EXPECT_CALL(hid_system_tray_icon(),
                  NotifyConnectionCountUpdated(profile()))
          .Times(2);
    }
    hid_connection_tracker().DecrementConnectionCount(
        origin_name_pairs[0].first);
    hid_connection_tracker().DecrementConnectionCount(
        origin_name_pairs[0].first);
    EXPECT_EQ(hid_connection_tracker().total_connection_count(), 1);
    EXPECT_THAT(hid_connection_tracker().origins(),
                UnorderedElementsAre(
                    Pair(origin_name_pairs[0].first,
                         OriginState(0, t1, origin_name_pairs[0].second)),
                    Pair(origin_name_pairs[1].first,
                         OriginState(1, t1, origin_name_pairs[1].second))));
    testing::Mock::VerifyAndClearExpectations(&hid_connection_tracker());

    task_environment()->FastForwardBy(base::Seconds(5));
    auto t6 = TimeTicks::Now();
    if (has_system_tray_icon) {
      EXPECT_CALL(hid_system_tray_icon(),
                  NotifyConnectionCountUpdated(profile()));
    }
    hid_connection_tracker().DecrementConnectionCount(
        origin_name_pairs[1].first);
    EXPECT_EQ(hid_connection_tracker().total_connection_count(), 0);
    EXPECT_THAT(hid_connection_tracker().origins(),
                UnorderedElementsAre(
                    Pair(origin_name_pairs[0].first,
                         OriginState(0, t1, origin_name_pairs[0].second)),
                    Pair(origin_name_pairs[1].first,
                         OriginState(0, t6, origin_name_pairs[1].second))));

    // The first origin is removed at t11.
    if (has_system_tray_icon) {
      EXPECT_CALL(hid_system_tray_icon(),
                  NotifyConnectionCountUpdated(profile()));
    }
    task_environment()->FastForwardBy(base::Seconds(5));
    auto t11 = TimeTicks::Now();
    EXPECT_EQ(hid_connection_tracker().total_connection_count(), 0);
    EXPECT_THAT(hid_connection_tracker().origins(),
                UnorderedElementsAre(
                    Pair(origin_name_pairs[1].first,
                         OriginState(0, t6, origin_name_pairs[1].second))));
    testing::Mock::VerifyAndClearExpectations(&hid_connection_tracker());

    // New connection on the second origin comes in at t11, so it won't be
    // removed at t16.
    if (has_system_tray_icon) {
      EXPECT_CALL(hid_system_tray_icon(),
                  NotifyConnectionCountUpdated(profile()));
    }
    hid_connection_tracker().IncrementConnectionCount(
        origin_name_pairs[1].first);
    EXPECT_EQ(hid_connection_tracker().total_connection_count(), 1);
    EXPECT_THAT(hid_connection_tracker().origins(),
                UnorderedElementsAre(
                    Pair(origin_name_pairs[1].first,
                         OriginState(1, t11, origin_name_pairs[1].second))));
    testing::Mock::VerifyAndClearExpectations(&hid_connection_tracker());
    task_environment()->FastForwardBy(base::Seconds(5));
    auto t16 = TimeTicks::Now();
    // Scheduled CleanUpOrigin is no-op at t16.
    if (has_system_tray_icon) {
      EXPECT_CALL(hid_system_tray_icon(),
                  NotifyConnectionCountUpdated(profile()))
          .Times(0);
      EXPECT_CALL(hid_system_tray_icon(),
                  UnstageProfile(profile(), /*immediate=*/true))
          .Times(0);
    }
    EXPECT_EQ(hid_connection_tracker().total_connection_count(), 1);
    EXPECT_THAT(hid_connection_tracker().origins(),
                UnorderedElementsAre(
                    Pair(origin_name_pairs[1].first,
                         OriginState(1, t11, origin_name_pairs[1].second))));
    testing::Mock::VerifyAndClearExpectations(&hid_connection_tracker());

    // The last connection of the second origin is gone at t16.
    if (has_system_tray_icon) {
      EXPECT_CALL(hid_system_tray_icon(),
                  NotifyConnectionCountUpdated(profile()));
    }
    hid_connection_tracker().DecrementConnectionCount(
        origin_name_pairs[1].first);
    EXPECT_EQ(hid_connection_tracker().total_connection_count(), 0);
    EXPECT_THAT(hid_connection_tracker().origins(),
                UnorderedElementsAre(
                    Pair(origin_name_pairs[1].first,
                         OriginState(0, t16, origin_name_pairs[1].second))));
    testing::Mock::VerifyAndClearExpectations(&hid_connection_tracker());

    // The second origin is removed at time t26, and the profile is removed from
    // the system tray icon because there are no active origins on this profile.
    if (has_system_tray_icon) {
      EXPECT_CALL(hid_system_tray_icon(),
                  UnstageProfile(profile(), /*immediate=*/true));
    }
    task_environment()->FastForwardBy(base::Seconds(10));
    EXPECT_EQ(hid_connection_tracker().total_connection_count(), 0);
    EXPECT_TRUE(hid_connection_tracker().origins().empty());
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  void TestDeviceConnectionExtensionOrigins(bool has_system_tray_icon) {
    auto extension1 = CreateExtensionWithName("Test Extension 1");
    auto extension2 = CreateExtensionWithName("Test Extension 2");
    AddExtensionToProfile(profile(), extension1.get());
    AddExtensionToProfile(profile(), extension2.get());
    TestDeviceConnection(has_system_tray_icon,
                         {{extension1->origin(), extension1->name()},
                          {extension2->origin(), extension2->name()}});
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

 private:
  raw_ptr<HidConnectionTracker> hid_connection_tracker_;
  raw_ptr<MockHidSystemTrayIcon> hid_system_tray_icon_;
};

#if BUILDFLAG(ENABLE_EXTENSIONS)
TEST_F(HidConnectionTrackerTest, DeviceConnectionExtensionOrigins) {
  TestDeviceConnectionExtensionOrigins(/*has_system_tray_icon=*/true);
}

// Test the scenario with null HID system tray icon and it doesn't cause crash.
TEST_F(HidConnectionTrackerTest,
       DeviceConnectionExtensionOriginsWithNullSystemTrayIcon) {
  TestingBrowserProcess::GetGlobal()->SetHidSystemTrayIcon(nullptr);
  TestDeviceConnectionExtensionOrigins(/*has_system_tray_icon=*/false);
}

TEST_F(HidConnectionTrackerTest, ProfileDestroyedExtensionOrigin) {
  auto t0 = TimeTicks::Now();
  auto* profile_to_be_destroyed = CreateTestingProfile(kTestProfileName);
  auto extension = CreateExtensionWithName("Test Extension");
  auto origin = extension->origin();
  auto* connection_tracker =
      HidConnectionTrackerFactory::GetForProfile(profile_to_be_destroyed,
                                                 /*create=*/true);
  AddExtensionToProfile(profile_to_be_destroyed, extension.get());

  EXPECT_CALL(hid_system_tray_icon(), StageProfile(profile_to_be_destroyed));
  connection_tracker->IncrementConnectionCount(origin);
  EXPECT_EQ(connection_tracker->total_connection_count(), 1);
  EXPECT_THAT(
      connection_tracker->origins(),
      UnorderedElementsAre(Pair(origin, OriginState(1, t0, "Test Extension"))));
  testing::Mock::VerifyAndClearExpectations(&hid_connection_tracker());

  EXPECT_CALL(hid_system_tray_icon(),
              NotifyConnectionCountUpdated(profile_to_be_destroyed));
  connection_tracker->DecrementConnectionCount(origin);
  EXPECT_EQ(connection_tracker->total_connection_count(), 0);
  EXPECT_THAT(
      connection_tracker->origins(),
      UnorderedElementsAre(Pair(origin, OriginState(0, t0, "Test Extension"))));
  testing::Mock::VerifyAndClearExpectations(&hid_connection_tracker());

  // The profile is destroyed at t5.
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_CALL(hid_system_tray_icon(),
              UnstageProfile(profile_to_be_destroyed, /*immediate=*/true));
  profile_manager()->DeleteTestingProfile(kTestProfileName);
  testing::Mock::VerifyAndClearExpectations(&hid_connection_tracker());

  // The connection tracker is destroyed when the profile is destroyed. No
  // UnstageProfile is sent to the system tray icon at time t10.
  EXPECT_CALL(hid_system_tray_icon(),
              UnstageProfile(profile_to_be_destroyed, /*immediate=*/true))
      .Times(0);
  task_environment()->FastForwardBy(base::Seconds(10));
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
