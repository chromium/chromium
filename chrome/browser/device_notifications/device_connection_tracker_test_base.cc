// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/device_notifications/device_connection_tracker_test_base.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/device_notifications/device_system_tray_icon.h"
#include "chrome/browser/device_notifications/device_system_tray_icon_renderer.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/profiles/delete_profile_helper.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/profile_deletion_observer.h"
#include "chrome/test/base/ui_test_utils.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/test_helper.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_builder.h"
#endif

namespace {

using base::TimeTicks;
using testing::Pair;
using testing::Return;
using testing::UnorderedElementsAre;

using OriginState = DeviceConnectionTracker::OriginState;

#if !BUILDFLAG(IS_CHROMEOS)
constexpr char kTestProfileName[] = "user@gmail.com";
#endif

}  // namespace

DeviceConnectionTrackerTestBase::DeviceConnectionTrackerTestBase() = default;
DeviceConnectionTrackerTestBase::~DeviceConnectionTrackerTestBase() = default;

#if BUILDFLAG(IS_CHROMEOS)
void DeviceConnectionTrackerTestBase::SetUpLocalStatePrefService(
    PrefService* local_state) {
  InProcessBrowserTest::SetUpLocalStatePrefService(local_state);

  // Register a persisted user.
  user_manager::TestHelper::RegisterPersistedUser(*local_state,
                                                  test_account_id_);
}

Profile& DeviceConnectionTrackerTestBase::StartUserSession(
    const AccountId& account_id) {
  auto* session_manager = session_manager::SessionManager::Get();
  session_manager->CreateSession(account_id, account_id.GetUserEmail(),
                                 /*new_user=*/false,
                                 /*has_active_session=*/false);

  Profile& profile = profiles::testing::CreateProfileSync(
      g_browser_process->profile_manager(),
      ash::BrowserContextHelper::Get()->GetBrowserContextPathByUserIdHash(
          user_manager::UserManager::Get()
              ->FindUser(account_id)
              ->username_hash()));

  session_manager->NotifyUserProfileLoaded(account_id);
  session_manager->SessionStarted();
  return profile;
}
#endif

void DeviceConnectionTrackerTestBase::TestDeviceConnection(
    bool has_system_tray_icon,
    const std::vector<std::pair<url::Origin, std::string>>& origin_name_pairs) {
  ASSERT_EQ(origin_name_pairs.size(), 2u);
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  auto t0 = task_runner->NowTicks();
  MockDeviceSystemTrayIcon* mock_device_system_tray_icon =
      GetMockDeviceSystemTrayIcon();
  auto* connection_tracker =
      GetDeviceConnectionTracker(browser()->profile(), true);
  connection_tracker->SetTaskRunnerAndClockForTesting(
      task_runner, task_runner->GetMockTickClock());

  // First connection of the first origin stages the profile.
  if (has_system_tray_icon) {
    EXPECT_CALL(*mock_device_system_tray_icon,
                StageProfile(browser()->profile()));
  }
  connection_tracker->IncrementConnectionCount(origin_name_pairs[0].first);
  EXPECT_EQ(connection_tracker->total_connection_count(), 1);
  EXPECT_THAT(connection_tracker->origins(),
              UnorderedElementsAre(
                  Pair(origin_name_pairs[0].first,
                       OriginState(1, t0, origin_name_pairs[0].second))));
  testing::Mock::VerifyAndClearExpectations(&connection_tracker);

  // The second origin comes in at t1.
  task_runner->FastForwardBy(base::Seconds(1));
  auto t1 = task_runner->NowTicks();
  if (has_system_tray_icon) {
    EXPECT_CALL(*mock_device_system_tray_icon,
                NotifyConnectionCountUpdated(browser()->profile()))
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
                NotifyConnectionCountUpdated(browser()->profile()))
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

  task_runner->FastForwardBy(base::Seconds(1));
  auto t2 = task_runner->NowTicks();
  if (has_system_tray_icon) {
    EXPECT_CALL(*mock_device_system_tray_icon,
                NotifyConnectionCountUpdated(browser()->profile()));
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
                NotifyConnectionCountUpdated(browser()->profile()));
  }
  task_runner->FastForwardBy(base::Seconds(2));
  auto t4 = task_runner->NowTicks();
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
                NotifyConnectionCountUpdated(browser()->profile()));
  }
  connection_tracker->IncrementConnectionCount(origin_name_pairs[1].first);
  EXPECT_EQ(connection_tracker->total_connection_count(), 1);
  EXPECT_THAT(connection_tracker->origins(),
              UnorderedElementsAre(
                  Pair(origin_name_pairs[1].first,
                       OriginState(1, t4, origin_name_pairs[1].second))));
  testing::Mock::VerifyAndClearExpectations(&connection_tracker);
  task_runner->FastForwardBy(base::Seconds(1));
  auto t5 = task_runner->NowTicks();
  // Scheduled CleanUpOrigin is no-op at t5.
  if (has_system_tray_icon) {
    EXPECT_CALL(*mock_device_system_tray_icon,
                NotifyConnectionCountUpdated(browser()->profile()))
        .Times(0);
    EXPECT_CALL(*mock_device_system_tray_icon,
                UnstageProfile(browser()->profile(), /*immediate=*/true))
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
                NotifyConnectionCountUpdated(browser()->profile()));
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
                UnstageProfile(browser()->profile(), /*immediate=*/true))
        .Times(1);
  }
  task_runner->FastForwardBy(base::Seconds(3));
  EXPECT_EQ(connection_tracker->total_connection_count(), 0);
  EXPECT_TRUE(connection_tracker->origins().empty());
}

void DeviceConnectionTrackerTestBase::TestWhitelistedOrigin(
    const std::pair<url::Origin, std::string> whitelisted_origin,
    const std::pair<url::Origin, std::string> non_whitelisted_origin) {
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  auto t0 = task_runner->NowTicks();
  MockDeviceSystemTrayIcon* mock_device_system_tray_icon =
      GetMockDeviceSystemTrayIcon();
  auto* connection_tracker =
      GetDeviceConnectionTracker(browser()->profile(), true);
  connection_tracker->SetTaskRunnerAndClockForTesting(
      task_runner, task_runner->GetMockTickClock());

  EXPECT_CALL(*mock_device_system_tray_icon, StageProfile(browser()->profile()))
      .Times(0);
  connection_tracker->IncrementConnectionCount(whitelisted_origin.first);
  EXPECT_EQ(connection_tracker->total_connection_count(), 0);
  testing::Mock::VerifyAndClearExpectations(&connection_tracker);

  EXPECT_CALL(*mock_device_system_tray_icon,
              StageProfile(browser()->profile()));
  connection_tracker->IncrementConnectionCount(non_whitelisted_origin.first);
  EXPECT_EQ(connection_tracker->total_connection_count(), 1);
  EXPECT_THAT(connection_tracker->origins(),
              UnorderedElementsAre(
                  Pair(non_whitelisted_origin.first,
                       OriginState(1, t0, non_whitelisted_origin.second))));
  testing::Mock::VerifyAndClearExpectations(&connection_tracker);

  EXPECT_CALL(*mock_device_system_tray_icon,
              UnstageProfile(browser()->profile(), /*immediate=*/true))
      .Times(0);
  connection_tracker->DecrementConnectionCount(whitelisted_origin.first);
  EXPECT_EQ(connection_tracker->total_connection_count(), 1);
  EXPECT_THAT(connection_tracker->origins(),
              UnorderedElementsAre(
                  Pair(non_whitelisted_origin.first,
                       OriginState(1, t0, non_whitelisted_origin.second))));

  EXPECT_CALL(*mock_device_system_tray_icon,
              NotifyConnectionCountUpdated(browser()->profile()));

  connection_tracker->DecrementConnectionCount(non_whitelisted_origin.first);
  EXPECT_CALL(*mock_device_system_tray_icon,
              UnstageProfile(browser()->profile(), /*immediate=*/true));

  task_runner->FastForwardBy(base::Seconds(3));
  EXPECT_EQ(connection_tracker->total_connection_count(), 0);
  EXPECT_TRUE(connection_tracker->origins().empty());
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
scoped_refptr<const extensions::Extension>
DeviceConnectionTrackerTestBase::CreateExtensionWithName(
    const std::string& extension_name) {
  base::DictValue manifest;
  manifest.Set("name", extension_name);
  manifest.Set("description", "For testing.");
  manifest.Set("version", "0.1");
  manifest.Set("manifest_version", 2);
  base::ListValue resources;
  resources.Append("index.html");
  manifest.Set("web_accessible_resources", std::move(resources));
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
  base::DictValue manifest;
  manifest.Set("name", extension_name);
  manifest.Set("description", "For testing.");
  manifest.Set("version", "0.1");
  manifest.Set("manifest_version", 2);
  base::ListValue resources;
  resources.Append("index.html");
  manifest.Set("web_accessible_resources", std::move(resources));
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
  extensions::ExtensionRegistrar::Get(profile)->AddExtension(extension);
}

void DeviceConnectionTrackerTestBase::TestDeviceConnectionExtensionOrigins(
    bool has_system_tray_icon) {
  auto extension1 = CreateExtensionWithName("Test Extension 1");
  auto extension2 = CreateExtensionWithName("Test Extension 2");
  AddExtensionToProfile(browser()->profile(), extension1.get());
  AddExtensionToProfile(browser()->profile(), extension2.get());
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
  AddExtensionToProfile(browser()->profile(), whitelisted_extension.get());
  AddExtensionToProfile(browser()->profile(), extension2.get());
  TestWhitelistedOrigin(
      {whitelisted_extension->origin(), whitelisted_extension->name()},
      {extension2->origin(), extension2->name()});
}

void DeviceConnectionTrackerTestBase::TestProfileDestroyedExtensionOrigin() {
  auto task_runner = base::MakeRefCounted<base::TestMockTimeTaskRunner>();
  auto t0 = task_runner->NowTicks();

  // Create a second profile.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
#if BUILDFLAG(IS_CHROMEOS)
  Profile& profile_to_be_destroyed_ref = StartUserSession(test_account_id_);
  Profile* profile_to_be_destroyed = &profile_to_be_destroyed_ref;
  base::FilePath profile_path = profile_to_be_destroyed->GetPath();
#else
  base::FilePath profile_path =
      profile_manager->user_data_dir().AppendASCII(kTestProfileName);
  Profile* profile_to_be_destroyed =
      &profiles::testing::CreateProfileSync(profile_manager, profile_path);
#endif

  auto extension = CreateExtensionWithName("Test Extension");
  auto origin = extension->origin();
  MockDeviceSystemTrayIcon* mock_device_system_tray_icon =
      GetMockDeviceSystemTrayIcon();
  auto* connection_tracker =
      GetDeviceConnectionTracker(profile_to_be_destroyed, true);
  connection_tracker->SetTaskRunnerAndClockForTesting(
      task_runner, task_runner->GetMockTickClock());
  AddExtensionToProfile(profile_to_be_destroyed, extension.get());

  EXPECT_CALL(*mock_device_system_tray_icon,
              StageProfile(profile_to_be_destroyed));
  connection_tracker->IncrementConnectionCount(origin);
  EXPECT_EQ(connection_tracker->total_connection_count(), 1);
  EXPECT_THAT(
      connection_tracker->origins(),
      UnorderedElementsAre(Pair(origin, OriginState(1, t0, "Test Extension"))));
  testing::Mock::VerifyAndClearExpectations(connection_tracker);

  EXPECT_CALL(*mock_device_system_tray_icon,
              NotifyConnectionCountUpdated(profile_to_be_destroyed));
  connection_tracker->DecrementConnectionCount(origin);
  EXPECT_EQ(connection_tracker->total_connection_count(), 0);
  EXPECT_THAT(
      connection_tracker->origins(),
      UnorderedElementsAre(Pair(origin, OriginState(0, t0, "Test Extension"))));
  testing::Mock::VerifyAndClearExpectations(connection_tracker);

  // The origin is cleaned up after 3 seconds, which triggers UnstageProfile.
  EXPECT_CALL(*mock_device_system_tray_icon,
              UnstageProfile(profile_to_be_destroyed, /*immediate=*/true))
      .Times(1);
  task_runner->FastForwardBy(base::Seconds(3));
  testing::Mock::VerifyAndClearExpectations(mock_device_system_tray_icon);

  // The profile is destroyed.
  ProfileDeletionObserver observer;
  profile_manager->GetDeleteProfileHelper().MaybeScheduleProfileForDeletion(
      profile_path, base::DoNothing(),
      ProfileMetrics::ProfileDelete::DELETE_PROFILE_USER_MANAGER);
  observer.Wait();

  // The connection tracker is destroyed when the profile is destroyed. No
  // UnstageProfile is sent to the system tray icon at time t3.
  EXPECT_CALL(*mock_device_system_tray_icon,
              UnstageProfile(profile_to_be_destroyed, /*immediate=*/true))
      .Times(0);
  task_runner->FastForwardBy(base::Seconds(1));
}

#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
