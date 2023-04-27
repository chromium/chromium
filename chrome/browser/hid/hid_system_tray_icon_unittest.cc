// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/hid/hid_system_tray_icon_unittest.h"

#include <memory>
#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/hid/hid_connection_tracker.h"
#include "chrome/browser/hid/hid_connection_tracker_factory.h"
#include "chrome/browser/hid/hid_system_tray_icon.h"
#include "chrome/grit/chromium_strings.h"
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
  NOTREACHED_NORETURN();
}

}  // namespace

MockHidConnectionTracker::MockHidConnectionTracker(Profile* profile)
    : HidConnectionTracker(profile) {}

MockHidConnectionTracker::~MockHidConnectionTracker() = default;

void HidSystemTrayIconTestBase::TearDown() {
  // In a test environment, g_browser_process is set to null before
  // TestingBrowserProcess is destroyed. This ensures that the tray icon is
  // destroyed before g_browser_process becomes null.
  TestingBrowserProcess::GetGlobal()->SetHidSystemTrayIcon(nullptr);
  BrowserWithTestWindowTest::TearDown();
}

std::u16string HidSystemTrayIconTestBase::GetExpectedTitle(
    size_t num_origins,
    size_t num_connections) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // The text might use "Google Chrome" or "Chromium" depending
  // is_chrome_branded in the build config file, hence using l10n_util to get
  // the expected string.
  if (num_origins == 1) {
    return l10n_util::GetPluralStringFUTF16(
        IDS_WEBHID_SYSTEM_TRAY_ICON_TITLE_SINGLE_EXTENSION,
        static_cast<int>(num_connections));
  }
  return l10n_util::GetPluralStringFUTF16(
      IDS_WEBHID_SYSTEM_TRAY_ICON_TITLE_MULTIPLE_EXTENSIONS,
      static_cast<int>(num_connections));
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
  NOTREACHED_NORETURN();
}

BrowserContextKeyedServiceFactory::TestingFactory
HidSystemTrayIconTestBase::GetHidConnectionTrackerTestingFactory() {
  return base::BindRepeating([](content::BrowserContext* browser_context) {
    return static_cast<std::unique_ptr<KeyedService>>(
        std::make_unique<MockHidConnectionTracker>(
            Profile::FromBrowserContext(browser_context)));
  });
}

Profile* HidSystemTrayIconTestBase::CreateTestingProfile(
    const std::string& profile_name) {
  // TODO(crbug.com/1399310): Pass testing factory when creating profile.
  // Ideally, we should be able to pass testing factory when calling profile
  // manager's CreateTestingProfile. However, due to the fact that:
  // 1) TestingProfile::TestingProfile(...) will call BrowserContextShutdown as
  //    part of setting testing factory.
  // 2) HidConnectionTrackerFactory::BrowserContextShutdown() at some point need
  //    valid profile_metrics::GetBrowserProfileType() as part of
  //    HidConnectionTrackerFactory::GetForProfile().
  // It will hit failure in profile_metrics::GetBrowserProfileType() due to
  // profile is not initialized properly before setting testing factory. As a
  // result, here create a profile then call SetTestingFactory to inject
  // MockHidConnectionTracker.
  Profile* profile = profile_manager()->CreateTestingProfile(profile_name);
  HidConnectionTrackerFactory::GetInstance()->SetTestingFactory(
      profile, GetHidConnectionTrackerTestingFactory());
  return profile;
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
scoped_refptr<const extensions::Extension>
HidSystemTrayIconTestBase::CreateExtensionWithName(
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

void HidSystemTrayIconTestBase::AddExtensionToProfile(
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

void HidSystemTrayIconTestBase::UnloadExtensionFromProfile(
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

void HidSystemTrayIconTestBase::TestSingleProfile(Profile* profile,
                                                  const url::Origin& origin1,
                                                  const url::Origin& origin2) {
  auto origin1_name = GetExpectedOriginName(profile, origin1);
  auto origin2_name = GetExpectedOriginName(profile, origin2);
  HidConnectionTracker* hid_connection_tracker =
      HidConnectionTrackerFactory::GetForProfile(profile, /*create=*/true);
  CheckIconHidden();

  hid_connection_tracker->IncrementConnectionCount(origin1);
  CheckIcon({{profile, {{origin1, 1, origin1_name}}}});

  hid_connection_tracker->IncrementConnectionCount(origin1);
  CheckIcon({{profile, {{origin1, 2, origin1_name}}}});

  hid_connection_tracker->IncrementConnectionCount(origin1);
  CheckIcon({{profile, {{origin1, 3, origin1_name}}}});

  hid_connection_tracker->IncrementConnectionCount(origin2);
  CheckIcon(
      {{profile, {{origin1, 3, origin1_name}, {origin2, 1, origin2_name}}}});

  hid_connection_tracker->DecrementConnectionCount(origin1);
  CheckIcon(
      {{profile, {{origin1, 2, origin1_name}, {origin2, 1, origin2_name}}}});

  hid_connection_tracker->DecrementConnectionCount(origin1);
  CheckIcon(
      {{profile, {{origin1, 1, origin1_name}, {origin2, 1, origin2_name}}}});

  // Two origins are removed 5 seconds apart.
  hid_connection_tracker->DecrementConnectionCount(origin2);
  CheckIcon(
      {{profile, {{origin1, 1, origin1_name}, {origin2, 0, origin2_name}}}});
  task_environment()->FastForwardBy(base::Seconds(5));
  hid_connection_tracker->DecrementConnectionCount(origin1);
  CheckIcon(
      {{profile, {{origin1, 0, origin1_name}, {origin2, 0, origin2_name}}}});
  task_environment()->FastForwardBy(base::Seconds(5));
  CheckIcon({{profile, {{origin1, 0, origin1_name}}}});
  task_environment()->FastForwardBy(base::Seconds(5));
  CheckIconHidden();
}

// Simulate a device connection bounce with a duration of 1 microsecond and
// ensure the profile lingers for 10 seconds.
void HidSystemTrayIconTestBase::TestBounceConnection(
    Profile* profile,
    const url::Origin& origin) {
  auto origin_name = GetExpectedOriginName(profile, origin);
  HidConnectionTracker* hid_connection_tracker =
      HidConnectionTrackerFactory::GetForProfile(profile, /*create=*/true);
  CheckIconHidden();

  hid_connection_tracker->IncrementConnectionCount(origin);
  CheckIcon({{profile, {{origin, 1, origin_name}}}});
  task_environment()->FastForwardBy(base::Nanoseconds(100));
  hid_connection_tracker->DecrementConnectionCount(origin);
  CheckIcon({{profile, {{origin, 0, origin_name}}}});
  task_environment()->FastForwardBy(base::Nanoseconds(100));
  hid_connection_tracker->IncrementConnectionCount(origin);
  CheckIcon({{profile, {{origin, 1, origin_name}}}});
  task_environment()->FastForwardBy(base::Nanoseconds(100));
  hid_connection_tracker->DecrementConnectionCount(origin);
  CheckIcon({{profile, {{origin, 0, origin_name}}}});
  task_environment()->FastForwardBy(base::Nanoseconds(100));
  hid_connection_tracker->IncrementConnectionCount(origin);
  CheckIcon({{profile, {{origin, 1, origin_name}}}});
  task_environment()->FastForwardBy(base::Nanoseconds(100));
  hid_connection_tracker->DecrementConnectionCount(origin);
  CheckIcon({{profile, {{origin, 0, origin_name}}}});
  task_environment()->FastForwardBy(HidConnectionTracker::kOriginInactiveTime);
  CheckIconHidden();
}

void HidSystemTrayIconTestBase::TestMultipleProfiles(
    const std::vector<
        std::pair<Profile*, std::vector<std::pair<url::Origin, std::string>>>>&
        profile_origins_pairs) {
  ASSERT_EQ(profile_origins_pairs.size(), 3u);
  std::vector<HidConnectionTracker*> hid_connection_trackers;
  for (const auto& [profile, origins] : profile_origins_pairs) {
    ASSERT_EQ(origins.size(), 2u);
    hid_connection_trackers.push_back(
        HidConnectionTrackerFactory::GetForProfile(profile, /*create=*/true));
  }
  CheckIconHidden();

  // Profile 1 has two connection on the first origin.
  hid_connection_trackers[0]->IncrementConnectionCount(
      profile_origins_pairs[0].second[0].first);
  hid_connection_trackers[0]->IncrementConnectionCount(
      profile_origins_pairs[0].second[0].first);
  CheckIcon({{profile_origins_pairs[0].first,
              {{profile_origins_pairs[0].second[0].first, 2,
                profile_origins_pairs[0].second[0].second}}}});

  // Profile 2 has one connection for each origin.
  hid_connection_trackers[1]->IncrementConnectionCount(
      profile_origins_pairs[1].second[0].first);
  hid_connection_trackers[1]->IncrementConnectionCount(
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
  hid_connection_trackers[2]->IncrementConnectionCount(
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

  // Destroyed a profile will remove it from being tracked in the hid system
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

  // The remaining two profiles are removed 5 seconds apart.
  hid_connection_trackers[2]->DecrementConnectionCount(
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

  task_environment()->FastForwardBy(base::Seconds(5));
  hid_connection_trackers[1]->DecrementConnectionCount(
      profile_origins_pairs[1].second[0].first);
  hid_connection_trackers[1]->DecrementConnectionCount(
      profile_origins_pairs[1].second[1].first);
  CheckIcon({{profile_origins_pairs[1].first,
              {{profile_origins_pairs[1].second[0].first, 0,
                profile_origins_pairs[1].second[0].second},
               {profile_origins_pairs[1].second[1].first, 0,
                profile_origins_pairs[1].second[1].second}}},
             {profile_origins_pairs[2].first,
              {{profile_origins_pairs[2].second[0].first, 0,
                profile_origins_pairs[2].second[0].second}}}});

  task_environment()->FastForwardBy(base::Seconds(5));
  CheckIcon({{profile_origins_pairs[1].first,
              {{profile_origins_pairs[1].second[0].first, 0,
                profile_origins_pairs[1].second[0].second},
               {profile_origins_pairs[1].second[1].first, 0,
                profile_origins_pairs[1].second[1].second}}}});
  task_environment()->FastForwardBy(base::Seconds(5));
  CheckIconHidden();
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void HidSystemTrayIconTestBase::TestSingleProfileExtentionOrigins() {
  Profile* profile = CreateTestingProfile("user");
  auto extension1 = CreateExtensionWithName("Test Extension 1");
  auto extension2 = CreateExtensionWithName("Test Extension 2");
  AddExtensionToProfile(profile, extension1.get());
  AddExtensionToProfile(profile, extension2.get());
  TestSingleProfile(profile, extension1->origin(), extension2->origin());
}

void HidSystemTrayIconTestBase::TestBounceConnectionExtensionOrigins() {
  Profile* profile = CreateTestingProfile("user");
  auto extension = CreateExtensionWithName("Test Extension");
  AddExtensionToProfile(profile, extension.get());
  TestBounceConnection(profile, extension->origin());
}

void HidSystemTrayIconTestBase::TestMultipleProfilesExtensionOrigins() {
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

void HidSystemTrayIconTestBase::TestExtensionRemoval() {
  Profile* profile = CreateTestingProfile("user");
  auto extension1 = CreateExtensionWithName("Test Extension 1");
  auto extension2 = CreateExtensionWithName("Test Extension 2");
  AddExtensionToProfile(profile, extension1.get());
  AddExtensionToProfile(profile, extension2.get());

  HidConnectionTracker* hid_connection_tracker =
      HidConnectionTrackerFactory::GetForProfile(profile, /*create=*/true);
  hid_connection_tracker->IncrementConnectionCount(extension1->origin());
  hid_connection_tracker->IncrementConnectionCount(extension2->origin());

  // The name remains available while it is in inactive duration, even if the
  // extension is removed.
  hid_connection_tracker->DecrementConnectionCount(extension1->origin());
  UnloadExtensionFromProfile(profile, extension1.get());
  // Removing extension2's connection will refresh the tray icon text. We want
  // to make sure it still shows extension1's name.
  hid_connection_tracker->DecrementConnectionCount(extension2->origin());
  CheckIcon({{profile,
              {{extension1->origin(), 0, "Test Extension 1"},
               {extension2->origin(), 0, "Test Extension 2"}}}});
}
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

class MockHidSystemTrayIcon : public HidSystemTrayIcon {
 public:
  MOCK_METHOD(void, ProfileAdded, (Profile*), (override));
  MOCK_METHOD(void, ProfileRemoved, (Profile*), (override));
  MOCK_METHOD(void, NotifyConnectionCountUpdated, (Profile*), (override));
};

class HidSystemTrayIconTest : public HidSystemTrayIconTestBase {
 public:
  void SetUp() override {
    HidSystemTrayIconTestBase::SetUp();
    hid_system_tray_icon_ = std::make_unique<MockHidSystemTrayIcon>();
  }

  // HidSystemTrayIconTestBase
  void CheckIcon(
      const std::vector<ProfileItem>& profile_connection_counts) override {}
  void CheckIconHidden() override {}

  MockHidSystemTrayIcon& hid_system_tray_icon() {
    return *hid_system_tray_icon_.get();
  }

 private:
  std::unique_ptr<MockHidSystemTrayIcon> hid_system_tray_icon_;
};

// A profile is scheduled to be removed followed by an immediate removal
// request.
TEST_F(HidSystemTrayIconTest, ScheduledRemovalFollowedByImmediateRemoval) {
  EXPECT_CALL(hid_system_tray_icon(), ProfileAdded(profile()));
  hid_system_tray_icon().StageProfile(profile());

  EXPECT_CALL(hid_system_tray_icon(), NotifyConnectionCountUpdated(profile()));
  hid_system_tray_icon().UnstageProfile(profile(), /*immediate*/ false);
  EXPECT_THAT(hid_system_tray_icon().GetProfilesForTesting(),
              UnorderedElementsAre(Pair(profile(), false)));

  EXPECT_CALL(hid_system_tray_icon(), ProfileRemoved(profile()));
  hid_system_tray_icon().UnstageProfile(profile(), /*immediate*/ true);
  EXPECT_TRUE(hid_system_tray_icon().GetProfilesForTesting().empty());
  testing::Mock::VerifyAndClearExpectations(&hid_system_tray_icon());

  EXPECT_CALL(hid_system_tray_icon(), ProfileRemoved(profile())).Times(0);
  task_environment()->FastForwardBy(HidSystemTrayIcon::kProfileUnstagingTime);
  EXPECT_TRUE(hid_system_tray_icon().GetProfilesForTesting().empty());
}

// A profile is scheduled to be removed then stage profile request comes in.
TEST_F(HidSystemTrayIconTest, ScheduledRemovalFollowedByStageProfile) {
  EXPECT_CALL(hid_system_tray_icon(), ProfileAdded(profile()));
  hid_system_tray_icon().StageProfile(profile());

  // NotifyConnectionCountUpdated is called twice: once when the profile is
  // unstaged, and again when the profile is staged.
  EXPECT_CALL(hid_system_tray_icon(), NotifyConnectionCountUpdated(profile()))
      .Times(2);
  hid_system_tray_icon().UnstageProfile(profile(), /*immediate=*/false);
  EXPECT_THAT(hid_system_tray_icon().GetProfilesForTesting(),
              UnorderedElementsAre(Pair(profile(), false)));
  EXPECT_CALL(hid_system_tray_icon(), ProfileAdded(profile())).Times(0);
  hid_system_tray_icon().StageProfile(profile());
  EXPECT_THAT(hid_system_tray_icon().GetProfilesForTesting(),
              UnorderedElementsAre(Pair(profile(), true)));

  EXPECT_CALL(hid_system_tray_icon(), ProfileRemoved(profile())).Times(0);
  task_environment()->FastForwardBy(HidSystemTrayIcon::kProfileUnstagingTime);
  EXPECT_THAT(hid_system_tray_icon().GetProfilesForTesting(),
              UnorderedElementsAre(Pair(profile(), true)));
}

// Two profiles removed 5 seconds apart.
TEST_F(HidSystemTrayIconTest, TwoProfilesRemovedFiveSecondsApart) {
  Profile* second_profile = CreateTestingProfile("user2");

  EXPECT_CALL(hid_system_tray_icon(), ProfileAdded(profile()));
  hid_system_tray_icon().StageProfile(profile());
  EXPECT_CALL(hid_system_tray_icon(), ProfileAdded(second_profile));
  hid_system_tray_icon().StageProfile(second_profile);

  EXPECT_CALL(hid_system_tray_icon(), NotifyConnectionCountUpdated(profile()));
  hid_system_tray_icon().UnstageProfile(profile(), /*immediate*/ false);
  EXPECT_THAT(
      hid_system_tray_icon().GetProfilesForTesting(),
      UnorderedElementsAre(Pair(profile(), false), Pair(second_profile, true)));

  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_CALL(hid_system_tray_icon(),
              NotifyConnectionCountUpdated(second_profile));
  hid_system_tray_icon().UnstageProfile(second_profile,
                                        /*immediate=*/false);
  EXPECT_THAT(hid_system_tray_icon().GetProfilesForTesting(),
              UnorderedElementsAre(Pair(profile(), false),
                                   Pair(second_profile, false)));

  // 10 seconds later, |profile| is removed.
  EXPECT_CALL(hid_system_tray_icon(), ProfileRemoved(profile()));
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_THAT(hid_system_tray_icon().GetProfilesForTesting(),
              UnorderedElementsAre(Pair(second_profile, false)));
  testing::Mock::VerifyAndClearExpectations(&hid_system_tray_icon());

  // 15 second later, |second_profile| is removed.
  EXPECT_CALL(hid_system_tray_icon(), ProfileRemoved(second_profile));
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_TRUE(hid_system_tray_icon().GetProfilesForTesting().empty());
}

// This test case just to make sure it can run through the scenario without
// causing any unexpected crash.
TEST_F(HidSystemTrayIconTest, CallbackAfterHidSystemTrayIconDestroyed) {
  Profile* profile = CreateTestingProfile("user");
  auto hid_system_tray_icon = std::make_unique<MockHidSystemTrayIcon>();
  EXPECT_CALL(*hid_system_tray_icon, ProfileAdded(profile));
  hid_system_tray_icon->StageProfile(profile);
  EXPECT_CALL(*hid_system_tray_icon, NotifyConnectionCountUpdated(profile));
  hid_system_tray_icon->UnstageProfile(profile, /*immediate*/ false);
  EXPECT_THAT(hid_system_tray_icon->GetProfilesForTesting(),
              UnorderedElementsAre(Pair(profile, false)));
  hid_system_tray_icon.reset();
  task_environment()->FastForwardBy(HidSystemTrayIcon::kProfileUnstagingTime);
}
