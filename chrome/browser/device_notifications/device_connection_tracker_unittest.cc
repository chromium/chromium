// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_notifications/device_connection_tracker_unittest.h"

#include <memory>

#include "chrome/browser/device_notifications/device_system_tray_icon.h"
#include "chrome/browser/device_notifications/device_system_tray_icon_renderer.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/testing_profile_manager.h"

namespace {

using base::TimeTicks;
using testing::Pair;
using testing::Return;
using testing::UnorderedElementsAre;

using OriginState = DeviceConnectionTracker::OriginState;

constexpr char kTestProfileName[] = "user@gmail.com";

}  // namespace

DeviceConnectionTrackerTestBase::DeviceConnectionTrackerTestBase()
    : BrowserWithTestWindowTest(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
DeviceConnectionTrackerTestBase::~DeviceConnectionTrackerTestBase() = default;

void DeviceConnectionTrackerTestBase::SetUp() {
  BrowserWithTestWindowTest::SetUp();
  BrowserList::SetLastActive(browser());
}

Profile* DeviceConnectionTrackerTestBase::CreateTestingProfile(
    const std::string& profile_name) {
  Profile* profile = profile_manager()->CreateTestingProfile(profile_name);
  return profile;
}

void DeviceConnectionTrackerTestBase::TestDeviceConnection(
    bool has_system_tray_icon,
    const std::vector<std::pair<url::Origin, std::string>>& origin_name_pairs) {
  ASSERT_EQ(origin_name_pairs.size(), 2u);
  auto t0 = TimeTicks::Now();
  MockDeviceSystemTrayIcon* mock_device_system_tray_icon =
      GetMockDeviceSystemTrayIcon();
  auto* connection_tracker = GetDeviceConnectionTracker(profile(), true);

  // First connection of the first origin stages the profile.
  if (has_system_tray_icon) {
    EXPECT_CALL(*mock_device_system_tray_icon, StageProfile(profile()));
  }
  connection_tracker->IncrementConnectionCount(origin_name_pairs[0].first);
  EXPECT_EQ(connection_tracker->total_connection_count(), 1);
  EXPECT_THAT(connection_tracker->origins(),
              UnorderedElementsAre(
                  Pair(origin_name_pairs[0].first,
                       OriginState(1, t0, origin_name_pairs[0].second))));
  testing::Mock::VerifyAndClearExpectations(&connection_tracker);

  // The second origin comes in at t1.
  task_environment()->FastForwardBy(base::Seconds(1));
  auto t1 = TimeTicks::Now();
  if (has_system_tray_icon) {
    EXPECT_CALL(*mock_device_system_tray_icon,
                NotifyConnectionCountUpdated(profile()))
        .Times(2);
  }
  connection_tracker->IncrementConnectionCount(origin_name_pairs[1].first);
  EXPECT_EQ(connection_tracker->total_connection_count(), 2);
  EXPECT_THAT(connection_tracker->origins(),
              UnorderedElementsAre(
                  Pair(origin_name_pairs[0].first,
                       OriginState(1, t0, origin_name_pairs[0].second)),
                  Pair(origin_name_pairs[1].first,
                       OriginState(1, t1, origin_name_pairs[1].second))));
  connection_tracker->IncrementConnectionCount(origin_name_pairs[0].first);
  EXPECT_EQ(connection_tracker->total_connection_count(), 3);
  EXPECT_THAT(connection_tracker->origins(),
              UnorderedElementsAre(
                  Pair(origin_name_pairs[0].first,
                       OriginState(2, t1, origin_name_pairs[0].second)),
                  Pair(origin_name_pairs[1].first,
                       OriginState(1, t1, origin_name_pairs[1].second))));
  testing::Mock::VerifyAndClearExpectations(&connection_tracker);

  // Two origins are removed 1 seconds apart.
  if (has_system_tray_icon) {
    EXPECT_CALL(*mock_device_system_tray_icon,
                NotifyConnectionCountUpdated(profile()))
        .Times(2);
  }
  connection_tracker->DecrementConnectionCount(origin_name_pairs[0].first);
  connection_tracker->DecrementConnectionCount(origin_name_pairs[0].first);
  EXPECT_EQ(connection_tracker->total_connection_count(), 1);
  EXPECT_THAT(connection_tracker->origins(),
              UnorderedElementsAre(
                  Pair(origin_name_pairs[0].first,
                       OriginState(0, t1, origin_name_pairs[0].second)),
                  Pair(origin_name_pairs[1].first,
                       OriginState(1, t1, origin_name_pairs[1].second))));
  testing::Mock::VerifyAndClearExpectations(&connection_tracker);

  task_environment()->FastForwardBy(base::Seconds(1));
  auto t2 = TimeTicks::Now();
  if (has_system_tray_icon) {
    EXPECT_CALL(*mock_device_system_tray_icon,
                NotifyConnectionCountUpdated(profile()));
  }
  connection_tracker->DecrementConnectionCount(origin_name_pairs[1].first);
  EXPECT_EQ(connection_tracker->total_connection_count(), 0);
  EXPECT_THAT(connection_tracker->origins(),
              UnorderedElementsAre(
                  Pair(origin_name_pairs[0].first,
                       OriginState(0, t1, origin_name_pairs[0].second)),
                  Pair(origin_name_pairs[1].first,
                       OriginState(0, t2, origin_name_pairs[1].second))));

  // The first origin is removed at t4.
  if (has_system_tray_icon) {
    EXPECT_CALL(*mock_device_system_tray_icon,
                NotifyConnectionCountUpdated(profile()));
  }
  task_environment()->FastForwardBy(base::Seconds(2));
  auto t4 = TimeTicks::Now();
  EXPECT_EQ(connection_tracker->total_connection_count(), 0);
  EXPECT_THAT(connection_tracker->origins(),
              UnorderedElementsAre(
                  Pair(origin_name_pairs[1].first,
                       OriginState(0, t2, origin_name_pairs[1].second))));
  testing::Mock::VerifyAndClearExpectations(&connection_tracker);

  // New connection on the second origin comes in at t4, so it won't be
  // removed at t5.
  if (has_system_tray_icon) {
    EXPECT_CALL(*mock_device_system_tray_icon,
                NotifyConnectionCountUpdated(profile()));
  }
  connection_tracker->IncrementConnectionCount(origin_name_pairs[1].first);
  EXPECT_EQ(connection_tracker->total_connection_count(), 1);
  EXPECT_THAT(connection_tracker->origins(),
              UnorderedElementsAre(
                  Pair(origin_name_pairs[1].first,
                       OriginState(1, t4, origin_name_pairs[1].second))));
  testing::Mock::VerifyAndClearExpectations(&connection_tracker);
  task_environment()->FastForwardBy(base::Seconds(1));
  auto t5 = TimeTicks::Now();
  // Scheduled CleanUpOrigin is no-op at t5.
  if (has_system_tray_icon) {
    EXPECT_CALL(*mock_device_system_tray_icon,
                NotifyConnectionCountUpdated(profile()))
        .Times(0);
    EXPECT_CALL(*mock_device_system_tray_icon,
                UnstageProfile(profile(), /*immediate=*/true))
        .Times(0);
  }
  EXPECT_EQ(connection_tracker->total_connection_count(), 1);
  EXPECT_THAT(connection_tracker->origins(),
              UnorderedElementsAre(
                  Pair(origin_name_pairs[1].first,
                       OriginState(1, t4, origin_name_pairs[1].second))));
  testing::Mock::VerifyAndClearExpectations(&connection_tracker);

  // The last connection of the second origin is gone at t5.
  if (has_system_tray_icon) {
    EXPECT_CALL(*mock_device_system_tray_icon,
                NotifyConnectionCountUpdated(profile()));
  }
  connection_tracker->DecrementConnectionCount(origin_name_pairs[1].first);
  EXPECT_EQ(connection_tracker->total_connection_count(), 0);
  EXPECT_THAT(connection_tracker->origins(),
              UnorderedElementsAre(
                  Pair(origin_name_pairs[1].first,
                       OriginState(0, t5, origin_name_pairs[1].second))));
  testing::Mock::VerifyAndClearExpectations(&connection_tracker);

  // The second origin is removed at time t8, and the profile is removed from
  // the system tray icon because there are no active origins on this profile.
  if (has_system_tray_icon) {
    EXPECT_CALL(*mock_device_system_tray_icon,
                UnstageProfile(profile(), /*immediate=*/true))
        .Times(1);
  }
  task_environment()->FastForwardBy(base::Seconds(3));
  EXPECT_EQ(connection_tracker->total_connection_count(), 0);
  EXPECT_TRUE(connection_tracker->origins().empty());
}

void DeviceConnectionTrackerTestBase::TestWhitelistedOrigin(
    const std::pair<url::Origin, std::string> whitelisted_origin,
    const std::pair<url::Origin, std::string> non_whitelisted_origin) {
  auto t0 = TimeTicks::Now();
  MockDeviceSystemTrayIcon* mock_device_system_tray_icon =
      GetMockDeviceSystemTrayIcon();
  auto* connection_tracker = GetDeviceConnectionTracker(profile(), true);

  EXPECT_CALL(*mock_device_system_tray_icon, StageProfile(profile())).Times(0);
  connection_tracker->IncrementConnectionCount(whitelisted_origin.first);
  EXPECT_EQ(connection_tracker->total_connection_count(), 0);
  testing::Mock::VerifyAndClearExpectations(&connection_tracker);

  EXPECT_CALL(*mock_device_system_tray_icon, StageProfile(profile()));
  connection_tracker->IncrementConnectionCount(non_whitelisted_origin.first);
  EXPECT_EQ(connection_tracker->total_connection_count(), 1);
  EXPECT_THAT(connection_tracker->origins(),
              UnorderedElementsAre(
                  Pair(non_whitelisted_origin.first,
                       OriginState(1, t0, non_whitelisted_origin.second))));
  testing::Mock::VerifyAndClearExpectations(&connection_tracker);

  EXPECT_CALL(*mock_device_system_tray_icon,
              UnstageProfile(profile(), /*immediate=*/true))
      .Times(0);
  connection_tracker->DecrementConnectionCount(whitelisted_origin.first);
  EXPECT_EQ(connection_tracker->total_connection_count(), 1);
  EXPECT_THAT(connection_tracker->origins(),
              UnorderedElementsAre(
                  Pair(non_whitelisted_origin.first,
                       OriginState(1, t0, non_whitelisted_origin.second))));

  EXPECT_CALL(*mock_device_system_tray_icon,
              NotifyConnectionCountUpdated(profile()));

  connection_tracker->DecrementConnectionCount(non_whitelisted_origin.first);
  EXPECT_CALL(*mock_device_system_tray_icon,
              UnstageProfile(profile(), /*immediate=*/true));

  task_environment()->FastForwardBy(base::Seconds(3));
  EXPECT_EQ(connection_tracker->total_connection_count(), 0);
  EXPECT_TRUE(connection_tracker->origins().empty());
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
scoped_refptr<const extensions::Extension>
DeviceConnectionTrackerTestBase::CreateExtensionWithName(
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

scoped_refptr<const extensions::Extension>
DeviceConnectionTrackerTestBase::CreateExtensionWithNameAndId(
    const std::string& extension_name,
    const std::string& extension_id) {
  auto manifest = base::Value::Dict()
                      .Set("name", extension_name)
                      .Set("description", "For testing.")
                      .Set("version", "0.1")
                      .Set("manifest_version", 2)
                      .Set("web_accessible_resources",
                           base::Value::List().Append("index.html"));
  scoped_refptr<const extensions::Extension> extension =
      extensions::ExtensionBuilder(/*name=*/extension_name)
          .SetID(extension_id)
          .MergeManifest(std::move(manifest))
          .Build();
  DCHECK(extension);
  return extension;
}

void DeviceConnectionTrackerTestBase::AddExtensionToProfile(
    Profile* profile,
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

void DeviceConnectionTrackerTestBase::TestDeviceConnectionExtensionOrigins(
    bool has_system_tray_icon) {
  auto extension1 = CreateExtensionWithName("Test Extension 1");
  auto extension2 = CreateExtensionWithName("Test Extension 2");
  AddExtensionToProfile(profile(), extension1.get());
  AddExtensionToProfile(profile(), extension2.get());
  TestDeviceConnection(has_system_tray_icon,
                       {{extension1->origin(), extension1->name()},
                        {extension2->origin(), extension2->name()}});
}

void DeviceConnectionTrackerTestBase::TestSingleProfileWhitelistedExtension(
    std::string whitelisted_extension_name,
    std::string whitelisted_extension_id) {
  auto whitelisted_extension = CreateExtensionWithNameAndId(
      whitelisted_extension_name, whitelisted_extension_id);
  auto extension2 = CreateExtensionWithName("Test Extension 2");
  AddExtensionToProfile(profile(), whitelisted_extension.get());
  AddExtensionToProfile(profile(), extension2.get());
  TestWhitelistedOrigin(
      {whitelisted_extension->origin(), whitelisted_extension->name()},
      {extension2->origin(), extension2->name()});
}

void DeviceConnectionTrackerTestBase::TestProfileDestroyedExtensionOrigin() {
  auto t0 = TimeTicks::Now();
  auto* profile_to_be_destroyed = CreateTestingProfile(kTestProfileName);
  auto extension = CreateExtensionWithName("Test Extension");
  auto origin = extension->origin();
  MockDeviceSystemTrayIcon* mock_device_system_tray_icon =
      GetMockDeviceSystemTrayIcon();
  auto* connection_tracker =
      GetDeviceConnectionTracker(profile_to_be_destroyed, true);
  auto* device_connection_tracker = GetDeviceConnectionTracker(profile(), true);
  AddExtensionToProfile(profile_to_be_destroyed, extension.get());

  EXPECT_CALL(*mock_device_system_tray_icon,
              StageProfile(profile_to_be_destroyed));
  connection_tracker->IncrementConnectionCount(origin);
  EXPECT_EQ(connection_tracker->total_connection_count(), 1);
  EXPECT_THAT(
      connection_tracker->origins(),
      UnorderedElementsAre(Pair(origin, OriginState(1, t0, "Test Extension"))));
  testing::Mock::VerifyAndClearExpectations(&device_connection_tracker);

  EXPECT_CALL(*mock_device_system_tray_icon,
              NotifyConnectionCountUpdated(profile_to_be_destroyed));
  connection_tracker->DecrementConnectionCount(origin);
  EXPECT_EQ(connection_tracker->total_connection_count(), 0);
  EXPECT_THAT(
      connection_tracker->origins(),
      UnorderedElementsAre(Pair(origin, OriginState(0, t0, "Test Extension"))));
  testing::Mock::VerifyAndClearExpectations(&device_connection_tracker);

  // The profile is destroyed at t2.
  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_CALL(*mock_device_system_tray_icon,
              UnstageProfile(profile_to_be_destroyed, /*immediate=*/true))
      .Times(1);
  profile_manager()->DeleteTestingProfile(kTestProfileName);
  testing::Mock::VerifyAndClearExpectations(&device_connection_tracker);

  // The connection tracker is destroyed when the profile is destroyed. No
  // UnstageProfile is sent to the system tray icon at time t3.
  EXPECT_CALL(*mock_device_system_tray_icon,
              UnstageProfile(profile_to_be_destroyed, /*immediate=*/true))
      .Times(0);
  task_environment()->FastForwardBy(base::Seconds(1));
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
