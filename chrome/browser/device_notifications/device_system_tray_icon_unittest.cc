// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_notifications/device_system_tray_icon_unittest.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/device_notifications/device_system_tray_icon.h"
#include "chrome/grit/branded_strings.h"
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

using testing::Pair;
using testing::UnorderedElementsAre;

const std::string& GetExpectedOriginName(Profile* profile,
                                         const url::Origin& origin) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (origin.scheme() == extensions::kExtensionScheme) {
    const auto* extension_registry =
        extensions::ExtensionRegistry::Get(profile);
    CHECK(extension_registry);
    const extensions::Extension* extension =
        extension_registry->GetExtensionById(
            origin.host(), extensions::ExtensionRegistry::EVERYTHING);
    CHECK(extension);
    return extension->name();
  }
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  NOTREACHED();
}

}  // namespace

void DeviceSystemTrayIconTestBase::TearDown() {
  // In a test environment, g_browser_process is set to null before
  // TestingBrowserProcess is destroyed. This ensures that the tray icon is
  // destroyed before g_browser_process becomes null.
  ResetTestingBrowserProcessSystemTrayIcon();
  BrowserWithTestWindowTest::TearDown();
}

Profile* DeviceSystemTrayIconTestBase::CreateTestingProfile(
    const std::string& profile_name) {
  // TODO(crbug.com/40249783): Pass testing factory when creating profile.
  // Ideally, we should be able to pass testing factory when calling profile
  // manager's CreateTestingProfile. However, due to the fact that:
  // 1) TestingProfile::TestingProfile(...) will call BrowserContextShutdown as
  //    part of setting testing factory.
  // 2) DeviceConnectionTrackerFactory::BrowserContextShutdown() at some point
  //    need valid profile_metrics::GetBrowserProfileType() as part of
  //    GetDeviceConnectionTracker().
  // It will hit failure in profile_metrics::GetBrowserProfileType() due to
  // profile is not initialized properly before setting testing factory. As a
  // result, here create a profile then call SetTestingFactory to inject
  // MockDeviceConnectionTracker.
  Profile* profile = profile_manager()->CreateTestingProfile(profile_name);
  SetDeviceConnectionTrackerTestingFactory(profile);
  return profile;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
scoped_refptr<const extensions::Extension>
DeviceSystemTrayIconTestBase::CreateExtensionWithName(
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
  DCHECK(extension);
  return extension;
}

void DeviceSystemTrayIconTestBase::AddExtensionToProfile(
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

void DeviceSystemTrayIconTestBase::UnloadExtensionFromProfile(
    Profile* profile,
    const extensions::Extension* extension) {
  extensions::TestExtensionSystem* extension_system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(profile));
  extensions::ExtensionService* extension_service =
      extension_system->extension_service();
  CHECK(extension_service);
  extension_service->UnloadExtension(
      extension->id(), extensions::UnloadedExtensionReason::UNINSTALL);
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

void DeviceSystemTrayIconTestBase::TestSingleProfile(
    Profile* profile,
    const url::Origin& origin1,
    const url::Origin& origin2) {
  auto origin1_name = GetExpectedOriginName(profile, origin1);
  auto origin2_name = GetExpectedOriginName(profile, origin2);
  DeviceConnectionTracker* connection_tracker =
      GetDeviceConnectionTracker(profile, /*create=*/true);
  CheckIconHidden();

  connection_tracker->IncrementConnectionCount(origin1);
  CheckIcon({{profile, {{origin1, 1, origin1_name}}}});

  connection_tracker->IncrementConnectionCount(origin1);
  CheckIcon({{profile, {{origin1, 2, origin1_name}}}});

  connection_tracker->IncrementConnectionCount(origin1);
  CheckIcon({{profile, {{origin1, 3, origin1_name}}}});

  connection_tracker->IncrementConnectionCount(origin2);
  CheckIcon(
      {{profile, {{origin1, 3, origin1_name}, {origin2, 1, origin2_name}}}});

  connection_tracker->DecrementConnectionCount(origin1);
  CheckIcon(
      {{profile, {{origin1, 2, origin1_name}, {origin2, 1, origin2_name}}}});

  connection_tracker->DecrementConnectionCount(origin1);
  CheckIcon(
      {{profile, {{origin1, 1, origin1_name}, {origin2, 1, origin2_name}}}});

  // Two origins are removed 2 seconds apart.
  connection_tracker->DecrementConnectionCount(origin2);
  CheckIcon(
      {{profile, {{origin1, 1, origin1_name}, {origin2, 0, origin2_name}}}});
  task_environment()->FastForwardBy(base::Seconds(2));
  connection_tracker->DecrementConnectionCount(origin1);
  CheckIcon(
      {{profile, {{origin1, 0, origin1_name}, {origin2, 0, origin2_name}}}});
  task_environment()->FastForwardBy(base::Seconds(1));
  CheckIcon({{profile, {{origin1, 0, origin1_name}}}});
  task_environment()->FastForwardBy(base::Seconds(2));
  CheckIconHidden();
}

// Simulate a device connection bounce with a duration of 1 microsecond and
// ensure the profile lingers for 10 seconds.
void DeviceSystemTrayIconTestBase::TestBounceConnection(
    Profile* profile,
    const url::Origin& origin) {
  auto origin_name = GetExpectedOriginName(profile, origin);
  DeviceConnectionTracker* connection_tracker =
      GetDeviceConnectionTracker(profile, /*create=*/true);
  CheckIconHidden();

  connection_tracker->IncrementConnectionCount(origin);
  CheckIcon({{profile, {{origin, 1, origin_name}}}});
  task_environment()->FastForwardBy(base::Nanoseconds(100));
  connection_tracker->DecrementConnectionCount(origin);
  CheckIcon({{profile, {{origin, 0, origin_name}}}});
  task_environment()->FastForwardBy(base::Nanoseconds(100));
  connection_tracker->IncrementConnectionCount(origin);
  CheckIcon({{profile, {{origin, 1, origin_name}}}});
  task_environment()->FastForwardBy(base::Nanoseconds(100));
  connection_tracker->DecrementConnectionCount(origin);
  CheckIcon({{profile, {{origin, 0, origin_name}}}});
  task_environment()->FastForwardBy(base::Nanoseconds(100));
  connection_tracker->IncrementConnectionCount(origin);
  CheckIcon({{profile, {{origin, 1, origin_name}}}});
  task_environment()->FastForwardBy(base::Nanoseconds(100));
  connection_tracker->DecrementConnectionCount(origin);
  CheckIcon({{profile, {{origin, 0, origin_name}}}});
  task_environment()->FastForwardBy(
      DeviceConnectionTracker::kOriginInactiveTime);
  CheckIconHidden();
}

void DeviceSystemTrayIconTestBase::TestMultipleProfiles(
    const std::vector<
        std::pair<Profile*, std::vector<std::pair<url::Origin, std::string>>>>&
        profile_origins_pairs) {
  ASSERT_EQ(profile_origins_pairs.size(), 3u);
  std::vector<DeviceConnectionTracker*> connection_trackers;
  for (const auto& [profile, origins] : profile_origins_pairs) {
    ASSERT_EQ(origins.size(), 2u);
    connection_trackers.push_back(
        GetDeviceConnectionTracker(profile, /*create=*/true));
  }
  CheckIconHidden();

  // Profile 1 has two connection on the first origin.
  connection_trackers[0]->IncrementConnectionCount(
      profile_origins_pairs[0].second[0].first);
  connection_trackers[0]->IncrementConnectionCount(
      profile_origins_pairs[0].second[0].first);
  CheckIcon({{profile_origins_pairs[0].first,
              {{profile_origins_pairs[0].second[0].first, 2,
                profile_origins_pairs[0].second[0].second}}}});

  // Profile 2 has one connection for each origin.
  connection_trackers[1]->IncrementConnectionCount(
      profile_origins_pairs[1].second[0].first);
  connection_trackers[1]->IncrementConnectionCount(
      profile_origins_pairs[1].second[1].first);
  CheckIcon({{profile_origins_pairs[0].first,
              {{profile_origins_pairs[1].second[0].first, 2,
                profile_origins_pairs[1].second[0].second}}},
             {profile_origins_pairs[1].first,
              {{profile_origins_pairs[1].second[0].first, 1,
                profile_origins_pairs[1].second[0].second},
               {profile_origins_pairs[1].second[1].first, 1,
                profile_origins_pairs[1].second[1].second}}}});

  // Profile 3 has one connection on the first origin.
  connection_trackers[2]->IncrementConnectionCount(
      profile_origins_pairs[2].second[0].first);
  CheckIcon({{profile_origins_pairs[0].first,
              {{profile_origins_pairs[1].second[0].first, 2,
                profile_origins_pairs[1].second[0].second}}},
             {profile_origins_pairs[1].first,
              {{profile_origins_pairs[1].second[0].first, 1,
                profile_origins_pairs[1].second[0].second},
               {profile_origins_pairs[1].second[1].first, 1,
                profile_origins_pairs[1].second[1].second}}},
             {profile_origins_pairs[2].first,
              {{profile_origins_pairs[2].second[0].first, 1,
                profile_origins_pairs[2].second[0].second}}}});

  // Destroyed a profile will remove it from being tracked in the device system
  // tray icon immediately.
  profile_manager()->DeleteTestingProfile(
      profile_origins_pairs[0].first->GetProfileUserName());
  CheckIcon({{profile_origins_pairs[1].first,
              {{profile_origins_pairs[1].second[0].first, 1,
                profile_origins_pairs[1].second[0].second},
               {profile_origins_pairs[1].second[1].first, 1,
                profile_origins_pairs[1].second[1].second}}},
             {profile_origins_pairs[2].first,
              {{profile_origins_pairs[2].second[0].first, 1,
                profile_origins_pairs[2].second[0].second}}}});

  // The remaining two profiles are removed 2 seconds apart.
  connection_trackers[2]->DecrementConnectionCount(
      profile_origins_pairs[2].second[0].first);
  // Connection count is updated immediately while the profile is scheduled
  // to be removed later.
  CheckIcon({{profile_origins_pairs[1].first,
              {{profile_origins_pairs[1].second[0].first, 1,
                profile_origins_pairs[1].second[0].second},
               {profile_origins_pairs[1].second[1].first, 1,
                profile_origins_pairs[1].second[1].second}}},
             {profile_origins_pairs[2].first,
              {{profile_origins_pairs[2].second[0].first, 0,
                profile_origins_pairs[2].second[0].second}}}});

  task_environment()->FastForwardBy(base::Seconds(2));
  connection_trackers[1]->DecrementConnectionCount(
      profile_origins_pairs[1].second[0].first);
  connection_trackers[1]->DecrementConnectionCount(
      profile_origins_pairs[1].second[1].first);
  CheckIcon({{profile_origins_pairs[1].first,
              {{profile_origins_pairs[1].second[0].first, 0,
                profile_origins_pairs[1].second[0].second},
               {profile_origins_pairs[1].second[1].first, 0,
                profile_origins_pairs[1].second[1].second}}},
             {profile_origins_pairs[2].first,
              {{profile_origins_pairs[2].second[0].first, 0,
                profile_origins_pairs[2].second[0].second}}}});

  task_environment()->FastForwardBy(base::Seconds(1));
  CheckIcon({{profile_origins_pairs[1].first,
              {{profile_origins_pairs[1].second[0].first, 0,
                profile_origins_pairs[1].second[0].second},
               {profile_origins_pairs[1].second[1].first, 0,
                profile_origins_pairs[1].second[1].second}}}});
  task_environment()->FastForwardBy(base::Seconds(2));
  CheckIconHidden();
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void DeviceSystemTrayIconTestBase::TestSingleProfileExtentionOrigins() {
  Profile* profile = CreateTestingProfile("user");
  auto extension1 = CreateExtensionWithName("Test Extension 1");
  auto extension2 = CreateExtensionWithName("Test Extension 2");
  AddExtensionToProfile(profile, extension1.get());
  AddExtensionToProfile(profile, extension2.get());
  TestSingleProfile(profile, extension1->origin(), extension2->origin());
}

void DeviceSystemTrayIconTestBase::TestBounceConnectionExtensionOrigins() {
  Profile* profile = CreateTestingProfile("user");
  auto extension = CreateExtensionWithName("Test Extension");
  AddExtensionToProfile(profile, extension.get());
  TestBounceConnection(profile, extension->origin());
}

void DeviceSystemTrayIconTestBase::TestMultipleProfilesExtensionOrigins() {
  std::vector<
      std::pair<Profile*, std::vector<std::pair<url::Origin, std::string>>>>
      profile_extensions_pairs;
  for (size_t idx = 0; idx < 3; idx++) {
    std::string profile_name = base::StringPrintf("user%zu", idx);
    auto* profile = CreateTestingProfile(profile_name);
    auto extension1 = CreateExtensionWithName("Test Extension 1");
    auto extension2 = CreateExtensionWithName("Test Extension 2");
    AddExtensionToProfile(profile, extension1.get());
    AddExtensionToProfile(profile, extension2.get());
    profile_extensions_pairs.push_back(
        {profile,
         {{extension1->origin(), extension1->name()},
          {extension2->origin(), extension2->name()}}});
  }
  TestMultipleProfiles(profile_extensions_pairs);
}

void DeviceSystemTrayIconTestBase::TestExtensionRemoval() {
  Profile* profile = CreateTestingProfile("user");
  auto extension1 = CreateExtensionWithName("Test Extension 1");
  auto extension2 = CreateExtensionWithName("Test Extension 2");
  AddExtensionToProfile(profile, extension1.get());
  AddExtensionToProfile(profile, extension2.get());

  DeviceConnectionTracker* connection_tracker =
      GetDeviceConnectionTracker(profile, /*create=*/true);
  connection_tracker->IncrementConnectionCount(extension1->origin());
  connection_tracker->IncrementConnectionCount(extension2->origin());

  // The name remains available while it is in inactive duration, even if the
  // extension is removed.
  connection_tracker->DecrementConnectionCount(extension1->origin());
  UnloadExtensionFromProfile(profile, extension1.get());
  // Removing extension2's connection will refresh the tray icon text. We want
  // to make sure it still shows extension1's name.
  connection_tracker->DecrementConnectionCount(extension2->origin());
  CheckIcon({{profile,
              {{extension1->origin(), 0, "Test Extension 1"},
               {extension2->origin(), 0, "Test Extension 2"}}}});
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

class DeviceSystemTrayIconTest : public DeviceSystemTrayIconTestBase {
 public:
  void SetUp() override {
    DeviceSystemTrayIconTestBase::SetUp();
    device_system_tray_icon_ = std::make_unique<MockDeviceSystemTrayIcon>();
    ON_CALL(*device_system_tray_icon_, StageProfile)
        .WillByDefault([this](Profile* profile) {
          device_system_tray_icon_->DeviceSystemTrayIcon::StageProfile(profile);
        });
    ON_CALL(*device_system_tray_icon_, UnstageProfile)
        .WillByDefault([this](Profile* profile, bool immediate) {
          device_system_tray_icon_->DeviceSystemTrayIcon::UnstageProfile(
              profile, immediate);
        });
  }

  // DeviceSystemTrayIconTestBase
  void CheckIcon(
      const std::vector<ProfileItem>& profile_connection_counts) override {}
  void CheckIconHidden() override {}
  void ResetTestingBrowserProcessSystemTrayIcon() override {}
  std::u16string GetExpectedTitle(size_t num_origins,
                                  size_t num_connections) override {
    return u"";
  }
  void SetDeviceConnectionTrackerTestingFactory(Profile* profile) override {}
  DeviceConnectionTracker* GetDeviceConnectionTracker(Profile* profile,
                                                      bool create) override {
    return nullptr;
  }
  MockDeviceConnectionTracker* GetMockDeviceConnectionTracker(
      DeviceConnectionTracker* connection_tracker) override {
    return nullptr;
  }

  MockDeviceSystemTrayIcon& device_system_tray_icon() {
    return *device_system_tray_icon_.get();
  }

 private:
  std::unique_ptr<MockDeviceSystemTrayIcon> device_system_tray_icon_;
};

// A profile is scheduled to be removed followed by an immediate removal
// request.
TEST_F(DeviceSystemTrayIconTest, ScheduledRemovalFollowedByImmediateRemoval) {
  EXPECT_CALL(device_system_tray_icon(), ProfileAdded(profile()));
  device_system_tray_icon().StageProfile(profile());

  EXPECT_CALL(device_system_tray_icon(),
              NotifyConnectionCountUpdated(profile()));
  device_system_tray_icon().UnstageProfile(profile(), /*immediate*/ false);
  EXPECT_THAT(device_system_tray_icon().GetProfilesForTesting(),
              UnorderedElementsAre(Pair(profile(), false)));

  EXPECT_CALL(device_system_tray_icon(), ProfileRemoved(profile()));
  device_system_tray_icon().UnstageProfile(profile(), /*immediate*/ true);
  EXPECT_TRUE(device_system_tray_icon().GetProfilesForTesting().empty());
  testing::Mock::VerifyAndClearExpectations(&device_system_tray_icon());

  EXPECT_CALL(device_system_tray_icon(), ProfileRemoved(profile())).Times(0);
  task_environment()->FastForwardBy(
      DeviceSystemTrayIcon::kProfileUnstagingTime);
  EXPECT_TRUE(device_system_tray_icon().GetProfilesForTesting().empty());
}

// A profile is scheduled to be removed then stage profile request comes in.
TEST_F(DeviceSystemTrayIconTest, ScheduledRemovalFollowedByStageProfile) {
  EXPECT_CALL(device_system_tray_icon(), ProfileAdded(profile()));
  device_system_tray_icon().StageProfile(profile());

  // NotifyConnectionCountUpdated is called twice: once when the profile is
  // unstaged, and again when the profile is staged.
  EXPECT_CALL(device_system_tray_icon(),
              NotifyConnectionCountUpdated(profile()))
      .Times(2);
  device_system_tray_icon().UnstageProfile(profile(), /*immediate=*/false);
  EXPECT_THAT(device_system_tray_icon().GetProfilesForTesting(),
              UnorderedElementsAre(Pair(profile(), false)));
  EXPECT_CALL(device_system_tray_icon(), ProfileAdded(profile())).Times(0);
  device_system_tray_icon().StageProfile(profile());
  EXPECT_THAT(device_system_tray_icon().GetProfilesForTesting(),
              UnorderedElementsAre(Pair(profile(), true)));

  EXPECT_CALL(device_system_tray_icon(), ProfileRemoved(profile())).Times(0);
  task_environment()->FastForwardBy(
      DeviceSystemTrayIcon::kProfileUnstagingTime);
  EXPECT_THAT(device_system_tray_icon().GetProfilesForTesting(),
              UnorderedElementsAre(Pair(profile(), true)));
}

// Two profiles removed 5 seconds apart.
TEST_F(DeviceSystemTrayIconTest, TwoProfilesRemovedFiveSecondsApart) {
  Profile* second_profile = CreateTestingProfile("user2");

  EXPECT_CALL(device_system_tray_icon(), ProfileAdded(profile()));
  device_system_tray_icon().StageProfile(profile());
  EXPECT_CALL(device_system_tray_icon(), ProfileAdded(second_profile));
  device_system_tray_icon().StageProfile(second_profile);

  EXPECT_CALL(device_system_tray_icon(),
              NotifyConnectionCountUpdated(profile()));
  device_system_tray_icon().UnstageProfile(profile(), /*immediate*/ false);
  EXPECT_THAT(
      device_system_tray_icon().GetProfilesForTesting(),
      UnorderedElementsAre(Pair(profile(), false), Pair(second_profile, true)));

  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_CALL(device_system_tray_icon(),
              NotifyConnectionCountUpdated(second_profile));
  device_system_tray_icon().UnstageProfile(second_profile,
                                           /*immediate=*/false);
  EXPECT_THAT(device_system_tray_icon().GetProfilesForTesting(),
              UnorderedElementsAre(Pair(profile(), false),
                                   Pair(second_profile, false)));

  // 10 seconds later, |profile| is removed.
  EXPECT_CALL(device_system_tray_icon(), ProfileRemoved(profile()));
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_THAT(device_system_tray_icon().GetProfilesForTesting(),
              UnorderedElementsAre(Pair(second_profile, false)));
  testing::Mock::VerifyAndClearExpectations(&device_system_tray_icon());

  // 15 second later, |second_profile| is removed.
  EXPECT_CALL(device_system_tray_icon(), ProfileRemoved(second_profile));
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(device_system_tray_icon().GetProfilesForTesting().empty());
}

// This test case just to make sure it can run through the scenario without
// causing any unexpected crash.
TEST_F(DeviceSystemTrayIconTest, CallbackAfterDeviceSystemTrayIconDestroyed) {
  Profile* profile = CreateTestingProfile("user");
  auto device_system_tray_icon = std::make_unique<MockDeviceSystemTrayIcon>();
  ON_CALL(*device_system_tray_icon, StageProfile)
      .WillByDefault([&device_system_tray_icon](Profile* profile) {
        device_system_tray_icon->DeviceSystemTrayIcon::StageProfile(profile);
      });
  ON_CALL(*device_system_tray_icon, UnstageProfile)
      .WillByDefault(
          [&device_system_tray_icon](Profile* profile, bool immediate) {
            device_system_tray_icon->DeviceSystemTrayIcon::UnstageProfile(
                profile, immediate);
          });
  EXPECT_CALL(*device_system_tray_icon, ProfileAdded(profile));
  device_system_tray_icon->StageProfile(profile);
  EXPECT_CALL(*device_system_tray_icon, NotifyConnectionCountUpdated(profile));
  device_system_tray_icon->UnstageProfile(profile, /*immediate*/ false);
  EXPECT_THAT(device_system_tray_icon->GetProfilesForTesting(),
              UnorderedElementsAre(Pair(profile, false)));
  device_system_tray_icon.reset();
  task_environment()->FastForwardBy(
      DeviceSystemTrayIcon::kProfileUnstagingTime);
}
