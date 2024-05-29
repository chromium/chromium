// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "chrome/browser/sync/test/integration/web_apps_sync_test_base.h"
#include "chrome/browser/web_applications/generated_icon_fix_manager.h"
#include "chrome/browser/web_applications/generated_icon_fix_util.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/fake_web_app_provider.h"
#include "chrome/browser/web_applications/test/fake_web_contents_manager.h"
#include "chrome/browser/web_applications/test/web_app_icon_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/browser/web_applications/web_app_sync_bridge.h"
#include "chrome/common/chrome_features.h"
#include "components/sync/base/features.h"
#include "components/sync/base/time.h"
#include "components/sync/service/sync_service_impl.h"
#include "content/public/test/browser_test.h"

namespace web_app {

// Used by GTEST for pretty printing in EXPECT_EQ.
static void PrintTo(const GeneratedIconFix& generated_icon_fix,
                    std::ostream* out) {
  *out << generated_icon_fix_util::ToDebugValue(&generated_icon_fix);
}

namespace {

struct GeneratedIconFixFutures {
  base::test::TestFuture<const webapps::AppId&,
                         GeneratedIconFixScheduleDecision>
      schedule;
  base::test::TestFuture<const webapps::AppId&, GeneratedIconFixResult> fix;

  explicit GeneratedIconFixFutures(FakeWebAppProvider& provider) {
    GeneratedIconFixManager& generated_icon_fix_manager =
        provider.generated_icon_fix_manager();
    generated_icon_fix_manager.maybe_schedule_callback_for_testing() =
        schedule.GetCallback();
    generated_icon_fix_manager.fix_completed_callback_for_testing() =
        fix.GetCallback();
  }
};

}  // namespace

class TwoClientGeneratedIconFixSyncTest : public WebAppsSyncTestBase {
 public:
  static GeneratedIconFix MakeGeneratedIconFix(
      GeneratedIconFixSource source,
      base::Time window_start_time,
      std::optional<base::Time> last_attempt_time,
      uint32_t attempt_count) {
    GeneratedIconFix generated_icon_fix;
    generated_icon_fix.set_source(source);
    generated_icon_fix.set_window_start_time(
        syncer::TimeToProtoTime(window_start_time));
    if (last_attempt_time.has_value()) {
      generated_icon_fix.set_last_attempt_time(
          syncer::TimeToProtoTime(last_attempt_time.value()));
    }
    generated_icon_fix.set_attempt_count(attempt_count);
    return generated_icon_fix;
  }

  TwoClientGeneratedIconFixSyncTest() : WebAppsSyncTestBase(TWO_CLIENT) {
    // Because the retry happens asynchronously it causes flakiness in the
    // metric expectations.
    GeneratedIconFixManager::DisableAutoRetryForTesting();
  }
  ~TwoClientGeneratedIconFixSyncTest() override = default;

  void SetUpOnMainThread() override {
    WebAppsSyncTestBase::SetUpOnMainThread();
    ASSERT_TRUE(SetupClients());
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Apps sync is controlled by a dedicated preference on Lacros,
    // corresponding to the Apps toggle in OS Sync settings.
    // Enable the Apps toggle for both clients.
    if (base::FeatureList::IsEnabled(syncer::kSyncChromeOSAppsToggleSharing)) {
      GetSyncService(0)->GetUserSettings()->SetAppsSyncEnabledByOs(true);
      GetSyncService(1)->GetUserSettings()->SetAppsSyncEnabledByOs(true);
    }
#endif
    ASSERT_TRUE(SetupSync());
  }

  void TearDownOnMainThread() override {
    fake_providers_.clear();
    WebAppsSyncTestBase::TearDownOnMainThread();
  }

  webapps::AppId SyncBrokenIcon(Profile* source, Profile* destination) {
    WebAppTestInstallObserver install_observer{destination};

    // Install on source profile.
    auto info = WebAppInstallInfo::CreateWithStartUrlForTesting(
        GURL("https://example.com"));
    info->title = u"Test name";
    info->description = u"Test description";
    info->manifest_icons.emplace_back(
        apps::IconInfo(GURL("https://example.com/icon.png"), 256));
    webapps::AppId app_id = test::InstallWebApp(source, std::move(info));

    // Wait for sync install on destination profile.
    install_observer.BeginListening({app_id});
    install_observer.Wait();

#if !BUILDFLAG(IS_CHROMEOS)
    // Install locally on destination profile.
    base::test::TestFuture<void> install_locally_future;
    fake_providers_[destination]->scheduler().InstallAppLocally(
        app_id, install_locally_future.GetCallback());
    EXPECT_TRUE(install_locally_future.Wait());
#endif  // !BUILDFLAG(IS_CHROMEOS)

    return app_id;
  }

  struct IconState {
    bool is_generated;
    bool is_correct_color;
    bool operator==(const IconState& other) const {
      return is_generated == other.is_generated &&
             is_correct_color == other.is_correct_color;
    }
  };
  IconState CheckIconState(Profile* profile, const webapps::AppId& app_id) {
    base::test::TestFuture<std::map<SquareSizePx, SkBitmap>> icons_future;
    fake_providers_[profile]->icon_manager().ReadIcons(
        app_id, IconPurpose::ANY, {256}, icons_future.GetCallback());
    return {
        .is_generated = fake_providers_[profile]
                            ->registrar_unsafe()
                            .GetAppById(app_id)
                            ->is_generated_icon(),
        .is_correct_color =
            icons_future.Get<0>().at(256).getColor(100, 100) == SK_ColorBLUE,
    };
  }

  void EnableIconServing(Profile* profile) {
    auto& fake_web_contents_manager = static_cast<FakeWebContentsManager&>(
        fake_providers_[profile]->web_contents_manager());
    FakeWebContentsManager::FakeIconState icon_state;
    icon_state.bitmaps.emplace_back(CreateSquareIcon(256, SK_ColorBLUE));
    fake_web_contents_manager.SetIconState(GURL("https://example.com/icon.png"),
                                           icon_state);
  }

  void SimulateRestart(FakeWebAppProvider& provider) {
    // It's difficult to set up the callback listener in time for a real restart
    // so directly clear any pending throttled fixes and re-invoke Start().
    provider.generated_icon_fix_manager().InvalidateWeakPtrsForTesting();
    provider.generated_icon_fix_manager().scheduled_fixes_for_testing().clear();
    provider.generated_icon_fix_manager().Start();
  }

 protected:
  FakeWebAppProviderCreator fake_web_app_provider_creator_{
      base::BindLambdaForTesting(
          [this](Profile* profile) -> std::unique_ptr<KeyedService> {
            auto fake_provider = std::make_unique<FakeWebAppProvider>(profile);

            fake_provider->CreateFakeSubsystems();
            // Use the real sync bridge to integrate with real sync logic.
            fake_provider->SetSyncBridge(std::make_unique<WebAppSyncBridge>(
                &fake_provider->GetRegistrarMutable()));
            fake_provider->StartWithSubsystems();

            fake_providers_[profile] = fake_provider.get();

            return fake_provider;
          })};

  base::flat_map<raw_ptr<Profile>, raw_ptr<FakeWebAppProvider>> fake_providers_;

  OsIntegrationManager::ScopedSuppressForTesting os_hooks_suppress_;

  base::test::ScopedFeatureList scoped_feature_list_{
      features::kWebAppSyncGeneratedIconBackgroundFix};
};

IN_PROC_BROWSER_TEST_F(TwoClientGeneratedIconFixSyncTest, Fix) {
  FakeWebAppProvider& provider1 = *fake_providers_[GetProfile(1)];
  base::HistogramTester histogram_tester;

  base::Time first_now = base::Time::Now();
  generated_icon_fix_util::SetNowForTesting(first_now);

  webapps::AppId app_id = SyncBrokenIcon(GetProfile(0), GetProfile(1));

  EXPECT_EQ(CheckIconState(GetProfile(1), app_id),
            (IconState{.is_generated = true, .is_correct_color = false}));
  EXPECT_EQ(
      provider1.registrar_unsafe().GetAppById(app_id)->generated_icon_fix(),
      MakeGeneratedIconFix(/*source=*/GeneratedIconFixSource_SYNC_INSTALL,
                           /*window_start_time=*/first_now,
                           /*last_attempt_time=*/std::nullopt,
                           /*attempt_count=*/0));

  EnableIconServing(GetProfile(1));
  base::Time second_now = first_now + base::Minutes(1);
  generated_icon_fix_util::SetNowForTesting(second_now);

  GeneratedIconFixFutures futures(provider1);

  SimulateRestart(provider1);

  EXPECT_EQ(futures.schedule.Get<webapps::AppId>(), app_id);
  EXPECT_EQ(futures.schedule.Get<GeneratedIconFixScheduleDecision>(),
            GeneratedIconFixScheduleDecision::kSchedule);
  EXPECT_EQ(futures.fix.Get<webapps::AppId>(), app_id);
  EXPECT_EQ(futures.fix.Get<GeneratedIconFixResult>(),
            GeneratedIconFixResult::kSuccess);

  EXPECT_EQ(CheckIconState(GetProfile(1), app_id),
            (IconState{.is_generated = false, .is_correct_color = true}));
  EXPECT_EQ(
      provider1.registrar_unsafe().GetAppById(app_id)->generated_icon_fix(),
      MakeGeneratedIconFix(/*source=*/GeneratedIconFixSource_SYNC_INSTALL,
                           /*window_start_time=*/first_now,
                           /*last_attempt_time=*/second_now,
                           /*attempt_count=*/1));

  histogram_tester.ExpectUniqueSample(
      "WebApp.GeneratedIconFix.ScheduleDecision",
      GeneratedIconFixScheduleDecision::kSchedule, 1);
  histogram_tester.ExpectUniqueSample("WebApp.GeneratedIconFix.Result",
                                      GeneratedIconFixResult::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "WebApp.GeneratedIconFix.StartUpAttemptCount", 1, 1);
  histogram_tester.ExpectUniqueSample("WebApp.GeneratedIconFix.AttemptCount", 0,
                                      1);
}

IN_PROC_BROWSER_TEST_F(TwoClientGeneratedIconFixSyncTest, TimeWindowExpired) {
  base::HistogramTester histogram_tester;
  FakeWebAppProvider& provider1 = *fake_providers_[GetProfile(1)];

  base::Time first_now = base::Time::Now();
  generated_icon_fix_util::SetNowForTesting(first_now);

  webapps::AppId app_id = SyncBrokenIcon(GetProfile(0), GetProfile(1));

  EXPECT_EQ(CheckIconState(GetProfile(1), app_id),
            (IconState{.is_generated = true, .is_correct_color = false}));
  EXPECT_EQ(
      provider1.registrar_unsafe().GetAppById(app_id)->generated_icon_fix(),
      MakeGeneratedIconFix(/*source=*/GeneratedIconFixSource_SYNC_INSTALL,
                           /*window_start_time=*/first_now,
                           /*last_attempt_time=*/std::nullopt,
                           /*attempt_count=*/0));

  EnableIconServing(GetProfile(1));
  base::Time second_now = first_now + base::Minutes(1);
  generated_icon_fix_util::SetNowForTesting(second_now);

  GeneratedIconFixFutures futures(provider1);

  // Simulate one week passing.
  base::Time third_now = second_now + base::Days(7);
  generated_icon_fix_util::SetNowForTesting(third_now);

  SimulateRestart(provider1);

  EXPECT_EQ(futures.schedule.Get<webapps::AppId>(), app_id);
  EXPECT_EQ(futures.schedule.Get<GeneratedIconFixScheduleDecision>(),
            GeneratedIconFixScheduleDecision::kTimeWindowExpired);
  EXPECT_FALSE(provider1.generated_icon_fix_manager()
                   .scheduled_fixes_for_testing()
                   .contains(app_id));

  EXPECT_EQ(CheckIconState(GetProfile(1), app_id),
            (IconState{.is_generated = true, .is_correct_color = false}));
  EXPECT_EQ(
      provider1.registrar_unsafe().GetAppById(app_id)->generated_icon_fix(),
      MakeGeneratedIconFix(/*source=*/GeneratedIconFixSource_SYNC_INSTALL,
                           /*window_start_time=*/first_now,
                           /*last_attempt_time=*/std::nullopt,
                           /*attempt_count=*/0));

  histogram_tester.ExpectUniqueSample(
      "WebApp.GeneratedIconFix.ScheduleDecision",
      GeneratedIconFixScheduleDecision::kTimeWindowExpired, 1);
  histogram_tester.ExpectTotalCount("WebApp.GeneratedIconFix.Result", 0);
  histogram_tester.ExpectUniqueSample(
      "WebApp.GeneratedIconFix.StartUpAttemptCount", 0, 1);
  histogram_tester.ExpectTotalCount("WebApp.GeneratedIconFix.AttemptCount", 0);
}

IN_PROC_BROWSER_TEST_F(TwoClientGeneratedIconFixSyncTest, NotRequired) {
  base::HistogramTester histogram_tester;
  FakeWebAppProvider& provider1 = *fake_providers_[GetProfile(1)];

  base::Time first_now = base::Time::Now();
  generated_icon_fix_util::SetNowForTesting(first_now);

  EnableIconServing(GetProfile(1));

  webapps::AppId app_id = SyncBrokenIcon(GetProfile(0), GetProfile(1));

  EXPECT_EQ(CheckIconState(GetProfile(1), app_id),
            (IconState{.is_generated = false, .is_correct_color = true}));
  EXPECT_EQ(
      provider1.registrar_unsafe().GetAppById(app_id)->generated_icon_fix(),
      MakeGeneratedIconFix(/*source=*/GeneratedIconFixSource_SYNC_INSTALL,
                           /*window_start_time=*/first_now,
                           /*last_attempt_time=*/std::nullopt,
                           /*attempt_count=*/0));

  GeneratedIconFixFutures futures(provider1);

  SimulateRestart(provider1);

  EXPECT_EQ(futures.schedule.Get<webapps::AppId>(), app_id);
  EXPECT_EQ(futures.schedule.Get<GeneratedIconFixScheduleDecision>(),
            GeneratedIconFixScheduleDecision::kNotRequired);
  EXPECT_FALSE(provider1.generated_icon_fix_manager()
                   .scheduled_fixes_for_testing()
                   .contains(app_id));

  EXPECT_EQ(
      provider1.registrar_unsafe().GetAppById(app_id)->generated_icon_fix(),
      MakeGeneratedIconFix(/*source=*/GeneratedIconFixSource_SYNC_INSTALL,
                           /*window_start_time=*/first_now,
                           /*last_attempt_time=*/std::nullopt,
                           /*attempt_count=*/0));

  histogram_tester.ExpectUniqueSample(
      "WebApp.GeneratedIconFix.ScheduleDecision",
      GeneratedIconFixScheduleDecision::kNotRequired, 1);
  histogram_tester.ExpectTotalCount("WebApp.GeneratedIconFix.Result", 0);
  histogram_tester.ExpectUniqueSample(
      "WebApp.GeneratedIconFix.StartUpAttemptCount", 0, 1);
  histogram_tester.ExpectTotalCount("WebApp.GeneratedIconFix.AttemptCount", 0);
}

IN_PROC_BROWSER_TEST_F(TwoClientGeneratedIconFixSyncTest, AppUninstalled) {
  base::HistogramTester histogram_tester;
  FakeWebAppProvider& provider1 = *fake_providers_[GetProfile(1)];

  base::Time first_now = base::Time::Now();
  generated_icon_fix_util::SetNowForTesting(first_now);

  webapps::AppId app_id = SyncBrokenIcon(GetProfile(0), GetProfile(1));

  EXPECT_EQ(CheckIconState(GetProfile(1), app_id),
            (IconState{.is_generated = true, .is_correct_color = false}));
  EXPECT_EQ(
      provider1.registrar_unsafe().GetAppById(app_id)->generated_icon_fix(),
      MakeGeneratedIconFix(/*source=*/GeneratedIconFixSource_SYNC_INSTALL,
                           /*window_start_time=*/first_now,
                           /*last_attempt_time=*/std::nullopt,
                           /*attempt_count=*/0));

  GeneratedIconFixFutures futures(provider1);

  SimulateRestart(provider1);

  EXPECT_EQ(futures.schedule.Get<webapps::AppId>(), app_id);
  EXPECT_EQ(futures.schedule.Get<GeneratedIconFixScheduleDecision>(),
            GeneratedIconFixScheduleDecision::kSchedule);

  // Delete the app before the command gets a chance to run.
  // Just delete right out of the registrar because injecting into the
  // command queue is difficult.
  provider1.sync_bridge_unsafe().BeginUpdate()->DeleteApp(app_id);

  EXPECT_EQ(futures.fix.Get<webapps::AppId>(), app_id);
  EXPECT_EQ(futures.fix.Get<GeneratedIconFixResult>(),
            GeneratedIconFixResult::kAppUninstalled);

  histogram_tester.ExpectUniqueSample(
      "WebApp.GeneratedIconFix.ScheduleDecision",
      GeneratedIconFixScheduleDecision::kSchedule, 1);
  histogram_tester.ExpectUniqueSample("WebApp.GeneratedIconFix.Result",
                                      GeneratedIconFixResult::kAppUninstalled,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "WebApp.GeneratedIconFix.StartUpAttemptCount", 1, 1);
  histogram_tester.ExpectUniqueSample("WebApp.GeneratedIconFix.AttemptCount", 0,
                                      1);
}

IN_PROC_BROWSER_TEST_F(TwoClientGeneratedIconFixSyncTest,
                       RetroactiveTimeWindow) {
  base::HistogramTester histogram_tester;
  FakeWebAppProvider& provider1 = *fake_providers_[GetProfile(1)];

  base::Time first_now = base::Time::Now();
  generated_icon_fix_util::SetNowForTesting(first_now);

  webapps::AppId app_id = SyncBrokenIcon(GetProfile(0), GetProfile(1));

  EXPECT_EQ(CheckIconState(GetProfile(1), app_id),
            (IconState{.is_generated = true, .is_correct_color = false}));
  EXPECT_EQ(
      provider1.registrar_unsafe().GetAppById(app_id)->generated_icon_fix(),
      MakeGeneratedIconFix(/*source=*/GeneratedIconFixSource_SYNC_INSTALL,
                           /*window_start_time=*/first_now,
                           /*last_attempt_time=*/std::nullopt,
                           /*attempt_count=*/0));

  const std::optional<GeneratedIconFix> generated_icon_fix =
      provider1.registrar_unsafe().GetAppById(app_id)->generated_icon_fix();
  // The fix time window should have started.
  ASSERT_TRUE(generated_icon_fix.has_value());
  // Delete the fix time window to simulate a web app install that happened
  // prior to updating to the GeneratedIconFix code.
  {
    provider1.sync_bridge_unsafe()
        .BeginUpdate()
        ->UpdateApp(app_id)
        ->SetGeneratedIconFix(std::nullopt);
  }

  // Fast forward time well beyond the fix time window.
  base::Time second_now = first_now + base::Days(1000);
  generated_icon_fix_util::SetNowForTesting(second_now);

  // The time window should start now.
  {
    GeneratedIconFixFutures futures(provider1);
    SimulateRestart(provider1);
    EXPECT_EQ(futures.schedule.Get<webapps::AppId>(), app_id);
    EXPECT_EQ(futures.schedule.Get<GeneratedIconFixScheduleDecision>(),
              GeneratedIconFixScheduleDecision::kSchedule);
    EXPECT_EQ(futures.fix.Get<webapps::AppId>(), app_id);
    EXPECT_EQ(futures.fix.Get<GeneratedIconFixResult>(),
              GeneratedIconFixResult::kStillGenerated);
  }
  EXPECT_EQ(
      provider1.registrar_unsafe().GetAppById(app_id)->generated_icon_fix(),
      MakeGeneratedIconFix(/*source=*/GeneratedIconFixSource_RETROACTIVE,
                           /*window_start_time=*/second_now,
                           /*last_attempt_time=*/second_now,
                           /*attempt_count=*/1));

  // Fast forward outside of the new time window.
  base::Time third_now = second_now + base::Days(7);
  generated_icon_fix_util::SetNowForTesting(third_now);

  // Check that the time window still expires.
  {
    GeneratedIconFixFutures futures(provider1);
    SimulateRestart(provider1);
    EXPECT_EQ(futures.schedule.Get<webapps::AppId>(), app_id);
    EXPECT_EQ(futures.schedule.Get<GeneratedIconFixScheduleDecision>(),
              GeneratedIconFixScheduleDecision::kTimeWindowExpired);
  }
  EXPECT_EQ(
      provider1.registrar_unsafe().GetAppById(app_id)->generated_icon_fix(),
      MakeGeneratedIconFix(/*source=*/GeneratedIconFixSource_RETROACTIVE,
                           /*window_start_time=*/second_now,
                           /*last_attempt_time=*/second_now,
                           /*attempt_count=*/1));

  histogram_tester.ExpectBucketCount(
      "WebApp.GeneratedIconFix.ScheduleDecision",
      GeneratedIconFixScheduleDecision::kSchedule, 1);
  histogram_tester.ExpectBucketCount(
      "WebApp.GeneratedIconFix.ScheduleDecision",
      GeneratedIconFixScheduleDecision::kTimeWindowExpired, 1);
  histogram_tester.ExpectUniqueSample("WebApp.GeneratedIconFix.Result",
                                      GeneratedIconFixResult::kStillGenerated,
                                      1);
  histogram_tester.ExpectBucketCount(
      "WebApp.GeneratedIconFix.StartUpAttemptCount", 0, 1);
  histogram_tester.ExpectBucketCount(
      "WebApp.GeneratedIconFix.StartUpAttemptCount", 1, 1);
  histogram_tester.ExpectBucketCount("WebApp.GeneratedIconFix.AttemptCount", 0,
                                     1);
}

IN_PROC_BROWSER_TEST_F(TwoClientGeneratedIconFixSyncTest, Throttling) {
  base::HistogramTester histogram_tester;
  FakeWebAppProvider& provider1 = *fake_providers_[GetProfile(1)];

  // Proto time conversion loses precision which must be accounted for when
  // calculating throttle duration.
  base::Time first_now =
      syncer::ProtoTimeToTime(syncer::TimeToProtoTime(base::Time::Now()));
  generated_icon_fix_util::SetNowForTesting(first_now);

  webapps::AppId app_id = SyncBrokenIcon(GetProfile(0), GetProfile(1));

  EXPECT_EQ(CheckIconState(GetProfile(1), app_id),
            (IconState{.is_generated = true, .is_correct_color = false}));
  EXPECT_EQ(
      provider1.registrar_unsafe().GetAppById(app_id)->generated_icon_fix(),
      MakeGeneratedIconFix(/*source=*/GeneratedIconFixSource_SYNC_INSTALL,
                           /*window_start_time=*/first_now,
                           /*last_attempt_time=*/std::nullopt,
                           /*attempt_count=*/0));

  base::Time second_now = first_now + base::Hours(1);
  generated_icon_fix_util::SetNowForTesting(second_now);

  // Trigger first fix attempt.
  {
    GeneratedIconFixFutures futures(provider1);

    SimulateRestart(provider1);

    EXPECT_EQ(futures.schedule.Get<webapps::AppId>(), app_id);
    EXPECT_EQ(futures.schedule.Get<GeneratedIconFixScheduleDecision>(),
              GeneratedIconFixScheduleDecision::kSchedule);
    EXPECT_EQ(futures.fix.Get<webapps::AppId>(), app_id);
    EXPECT_EQ(futures.fix.Get<GeneratedIconFixResult>(),
              GeneratedIconFixResult::kStillGenerated);
  }

  EXPECT_EQ(CheckIconState(GetProfile(1), app_id),
            (IconState{.is_generated = true, .is_correct_color = false}));
  const WebApp& app = *provider1.registrar_unsafe().GetAppById(app_id);
  EXPECT_EQ(app.generated_icon_fix(),
            MakeGeneratedIconFix(/*source=*/GeneratedIconFixSource_SYNC_INSTALL,
                                 /*window_start_time=*/first_now,
                                 /*last_attempt_time=*/second_now,
                                 /*attempt_count=*/1));

  base::Time third_now = second_now + base::Hours(1);
  generated_icon_fix_util::SetNowForTesting(third_now);

  // Next attempt should be throttled.
  EXPECT_EQ(generated_icon_fix_util::GetThrottleDuration(app), base::Hours(23));

  // Attempt should get scheduled anyway.
  {
    GeneratedIconFixFutures futures(provider1);

    SimulateRestart(provider1);

    EXPECT_EQ(futures.schedule.Get<webapps::AppId>(), app_id);
    EXPECT_EQ(futures.schedule.Get<GeneratedIconFixScheduleDecision>(),
              GeneratedIconFixScheduleDecision::kSchedule);
  }

  base::Time fourth_now = third_now + base::Days(1);
  generated_icon_fix_util::SetNowForTesting(fourth_now);

  // Next attempt should be unthrottled (were it not already scheduled).
  EXPECT_EQ(generated_icon_fix_util::GetThrottleDuration(app),
            base::TimeDelta());

  histogram_tester.ExpectUniqueSample(
      "WebApp.GeneratedIconFix.ScheduleDecision",
      GeneratedIconFixScheduleDecision::kSchedule, 2);
  histogram_tester.ExpectUniqueSample("WebApp.GeneratedIconFix.Result",
                                      GeneratedIconFixResult::kStillGenerated,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "WebApp.GeneratedIconFix.StartUpAttemptCount", 1, 2);
  histogram_tester.ExpectBucketCount("WebApp.GeneratedIconFix.AttemptCount", 0,
                                     1);
  histogram_tester.ExpectBucketCount("WebApp.GeneratedIconFix.AttemptCount", 1,
                                     1);

  // Note that this test doesn't use
  // TaskEnvironment(base::test::TaskEnvironment::TimeSource::MOCK_TIME) to
  // properly test the PostDelayedTask() behaviour due to the incompatibility it
  // has with the existing RunLoops and TestFutures that the rest of the test
  // relies on.
}

IN_PROC_BROWSER_TEST_F(TwoClientGeneratedIconFixSyncTest, AttemptLimit) {
  base::HistogramTester histogram_tester;
  FakeWebAppProvider& provider1 = *fake_providers_[GetProfile(1)];

  base::Time first_now = base::Time::Now();
  generated_icon_fix_util::SetNowForTesting(first_now);

  webapps::AppId app_id = SyncBrokenIcon(GetProfile(0), GetProfile(1));

  EXPECT_EQ(CheckIconState(GetProfile(1), app_id),
            (IconState{.is_generated = true, .is_correct_color = false}));
  EXPECT_EQ(
      provider1.registrar_unsafe().GetAppById(app_id)->generated_icon_fix(),
      MakeGeneratedIconFix(/*source=*/GeneratedIconFixSource_SYNC_INSTALL,
                           /*window_start_time=*/first_now,
                           /*last_attempt_time=*/std::nullopt,
                           /*attempt_count=*/0));

  // Fake there being (limit - 1) attempts.
  {
    provider1.sync_bridge_unsafe()
        .BeginUpdate()
        ->UpdateApp(app_id)
        ->SetGeneratedIconFix(
            MakeGeneratedIconFix(/*source=*/GeneratedIconFixSource_SYNC_INSTALL,
                                 /*window_start_time=*/first_now,
                                 /*last_attempt_time=*/first_now,
                                 /*attempt_count=*/6));
  }

  base::Time second_now = first_now + base::Days(1);
  generated_icon_fix_util::SetNowForTesting(second_now);

  // Trigger attempt that will fail.
  {
    GeneratedIconFixFutures futures(provider1);

    SimulateRestart(provider1);

    EXPECT_EQ(futures.schedule.Get<webapps::AppId>(), app_id);
    EXPECT_EQ(futures.schedule.Get<GeneratedIconFixScheduleDecision>(),
              GeneratedIconFixScheduleDecision::kSchedule);
    EXPECT_EQ(futures.fix.Get<webapps::AppId>(), app_id);
    EXPECT_EQ(futures.fix.Get<GeneratedIconFixResult>(),
              GeneratedIconFixResult::kStillGenerated);
  }

  EXPECT_EQ(CheckIconState(GetProfile(1), app_id),
            (IconState{.is_generated = true, .is_correct_color = false}));
  const WebApp& app = *provider1.registrar_unsafe().GetAppById(app_id);
  EXPECT_EQ(app.generated_icon_fix(),
            MakeGeneratedIconFix(/*source=*/GeneratedIconFixSource_SYNC_INSTALL,
                                 /*window_start_time=*/first_now,
                                 /*last_attempt_time=*/second_now,
                                 /*attempt_count=*/7));

  base::Time third_now = second_now + base::Days(1);
  generated_icon_fix_util::SetNowForTesting(third_now);

  // Next attempt should be denied.
  {
    GeneratedIconFixFutures futures(provider1);

    SimulateRestart(provider1);

    EXPECT_EQ(futures.schedule.Get<webapps::AppId>(), app_id);
    EXPECT_EQ(futures.schedule.Get<GeneratedIconFixScheduleDecision>(),
              GeneratedIconFixScheduleDecision::kAttemptLimitReached);
  }

  histogram_tester.ExpectBucketCount(
      "WebApp.GeneratedIconFix.ScheduleDecision",
      GeneratedIconFixScheduleDecision::kSchedule, 1);
  histogram_tester.ExpectBucketCount(
      "WebApp.GeneratedIconFix.ScheduleDecision",
      GeneratedIconFixScheduleDecision::kAttemptLimitReached, 1);
  histogram_tester.ExpectUniqueSample("WebApp.GeneratedIconFix.Result",
                                      GeneratedIconFixResult::kStillGenerated,
                                      1);
  histogram_tester.ExpectBucketCount(
      "WebApp.GeneratedIconFix.StartUpAttemptCount", 1, 1);
  histogram_tester.ExpectBucketCount(
      "WebApp.GeneratedIconFix.StartUpAttemptCount", 0, 1);
  histogram_tester.ExpectBucketCount("WebApp.GeneratedIconFix.AttemptCount", 6,
                                     1);
}

}  // namespace web_app
