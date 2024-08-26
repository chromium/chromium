// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/personalization_app/time_of_day_test_utils.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/shell.h"
#include "ash/system/geolocation/geolocation_controller.h"
#include "ash/system/geolocation/test_geolocation_url_loader_factory.h"
#include "ash/system/scheduled_feature/scheduled_feature.h"
#include "ash/wallpaper/test_wallpaper_image_downloader.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/wallpaper/wallpaper_time_of_day_scheduler.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_online_variant_utils.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/test_personalization_app_webui_provider.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/ash/wallpaper_handlers/mock_wallpaper_handlers.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chromeos/ash/components/geolocation/geoposition.h"
#include "chromeos/ash/components/geolocation/simple_geolocation_provider.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"

namespace ash::personalization_app {

namespace {

// Roughly the coordinates of Google London. In UTC time so timezones are easier
// to follow.
constexpr double kFakeLatitude = 51.493;
constexpr double kFakeLongitude = -0.216;

base::Time TimeFromString(const char* time_string) {
  base::Time time;
  CHECK(base::Time::FromUTCString(time_string, &time));
  return time;
}

Geoposition MakeGeoposition(const char* time_string) {
  Geoposition position;
  position.latitude = kFakeLatitude;
  position.longitude = kFakeLongitude;
  position.status = Geoposition::STATUS_OK;
  position.accuracy = 10;
  position.timestamp = TimeFromString(time_string);
  return position;
}

// Helper class to block until wallpaper colors have updated.
class WallpaperChangedWaiter : public WallpaperControllerObserver {
 public:
  explicit WallpaperChangedWaiter(base::OnceClosure on_wallpaper_changed)
      : on_wallpaper_changed_(std::move(on_wallpaper_changed)) {
    wallpaper_controller_observation_.Observe(WallpaperController::Get());
  }

  WallpaperChangedWaiter(const WallpaperChangedWaiter&) = delete;
  WallpaperChangedWaiter& operator=(const WallpaperChangedWaiter&) = delete;

  ~WallpaperChangedWaiter() override = default;

  void OnWallpaperChanged() override {
    if (on_wallpaper_changed_) {
      std::move(on_wallpaper_changed_).Run();
    }
  }

 private:
  base::OnceClosure on_wallpaper_changed_;
  base::ScopedObservation<WallpaperController, WallpaperControllerObserver>
      wallpaper_controller_observation_{this};
};

struct TimeOfDayTestParams {
  std::vector<const char*> timestamps_to_test;
  std::vector<ScheduleCheckpoint> expected_schedule_checkpoints;
  std::vector<backdrop::Image::ImageType> expected_image_types;
  Geoposition geoposition;
};

class PersonalizationAppTimeOfDayBrowserTest
    : public SystemWebAppBrowserTestBase,
      public ScheduledFeature::Clock,
      public testing::WithParamInterface<TimeOfDayTestParams> {
 public:
  PersonalizationAppTimeOfDayBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        personalization_app::GetTimeOfDayEnabledFeatures(), {});
    base::Time start_time = StartTime();
    clock_.SetNow(start_time);
    tick_clock_.SetNowTicks(base::TimeTicks() + (start_time - base::Time()));
  }

  PersonalizationAppTimeOfDayBrowserTest(
      const PersonalizationAppTimeOfDayBrowserTest&) = delete;
  PersonalizationAppTimeOfDayBrowserTest& operator=(
      const PersonalizationAppTimeOfDayBrowserTest&) = delete;

  ~PersonalizationAppTimeOfDayBrowserTest() override = default;

  // BrowserTestBase:
  void SetUpInProcessBrowserTestFixture() override {
    WallpaperControllerImpl::SetWallpaperImageDownloaderForTesting(
        std::make_unique<TestWallpaperImageDownloader>());
    SystemWebAppBrowserTestBase::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    SystemWebAppBrowserTestBase::SetUpOnMainThread();

    browser()->window()->Minimize();

    WallpaperControllerClientImpl::Get()->SetWallpaperFetcherDelegateForTesting(
        std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>());

    auto wallpaper_controller_test_api =
        std::make_unique<WallpaperControllerTestApi>(
            ::ash::Shell::Get()->wallpaper_controller());
    wallpaper_controller_test_api->SetDefaultWallpaper(
        GetAccountId(browser()->profile()));

    test_chrome_webui_controller_factory_.AddFactoryOverride(
        kChromeUIPersonalizationAppHost, &test_webui_provider_);

    time_of_day_scheduler_ = Shell::Get()
                                 ->wallpaper_controller()
                                 ->time_of_day_scheduler_for_testing();
    // Disable any running timers to set a fake clock.
    time_of_day_scheduler_->SetScheduleType(ScheduleType::kNone);
    time_of_day_scheduler_->SetClockForTesting(this);
    // Re-enable auto schedule and sun timer.
    time_of_day_scheduler_->SetScheduleType(ScheduleType::kSunsetToSunrise);

    auto* geolocation_controller = GeolocationController::Get();
    geolocation_controller->SetClockForTesting(this);

    // Override SharedUrlLoaderFactory to return fixed geoposition.
    scoped_refptr<TestGeolocationUrlLoaderFactory>
        geolocation_url_loader_factory =
            base::MakeRefCounted<TestGeolocationUrlLoaderFactory>();
    geolocation_url_loader_factory->set_position(GetGeoposition());
    SimpleGeolocationProvider::GetInstance()
        ->SetSharedUrlLoaderFactoryForTesting(geolocation_url_loader_factory);
    // Request immediate geoposition to fetch and broadcast the fixed
    // geoposition set by TestSharedUrlLoaderFactory above.
    GeolocationController::Get()->RequestImmediateGeopositionForTesting();

    WaitForTestSystemAppInstall();
  }

  void TearDownOnMainThread() override {
    SystemWebAppBrowserTestBase::TearDownOnMainThread();

    time_of_day_scheduler_ = nullptr;
  }

  content::WebContents* LaunchAppAtWallpaperSubpage(Browser** browser) {
    apps::AppLaunchParams launch_params =
        LaunchParamsForApp(ash::SystemWebAppType::PERSONALIZATION);
    launch_params.override_url =
        GURL(std::string(kChromeUIPersonalizationAppURL) +
             kWallpaperSubpageRelativeUrl);
    return LaunchApp(std::move(launch_params), browser);
  }

  // ScheduledFeature::Clock:
  base::Time Now() const override { return clock_.Now(); }

  base::TimeTicks NowTicks() const override { return tick_clock_.NowTicks(); }

  base::Time StartTime() { return times_to_test_.front(); }

  const std::vector<ScheduleCheckpoint>& ExpectedScheduleCheckpoints() {
    return GetParam().expected_schedule_checkpoints;
  }

  const std::vector<backdrop::Image::ImageType>& ExpectedImageTypes() {
    return GetParam().expected_image_types;
  }

  // Skip the first timestamp while iterating because the test starts there.
  std::vector<base::Time> TimesToIterate() {
    std::vector<base::Time> result;
    std::copy(times_to_test_.begin() + 1, times_to_test_.end(),
              std::back_inserter(result));
    return result;
  }

  const Geoposition& GetGeoposition() { return GetParam().geoposition; }

  // Returns whether the time_delta triggered a checkpoint change.
  bool FastForwardBy(base::TimeDelta time_delta) {
    clock_.Advance(time_delta);
    tick_clock_.Advance(time_delta);
    const bool checkpoint_should_change =
        time_of_day_scheduler_->timer()->IsRunning() &&
        time_of_day_scheduler_->timer()->desired_run_time() < NowTicks();
    if (checkpoint_should_change) {
      time_of_day_scheduler_->timer()->FireNow();
    }
    return checkpoint_should_change;
  }

  ScheduleCheckpoint GetCurrentCheckpoint() {
    return time_of_day_scheduler_->current_checkpoint();
  }

 private:
  std::vector<base::Time> GenerateTimesToTest() {
    const auto& timestamps = GetParam().timestamps_to_test;
    std::vector<base::Time> times;
    base::ranges::transform(timestamps, std::back_inserter(times),
                            TimeFromString);
    return times;
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  const std::vector<base::Time> times_to_test_ = GenerateTimesToTest();
  base::SimpleTestClock clock_;
  base::SimpleTestTickClock tick_clock_;
  raw_ptr<WallpaperTimeOfDayScheduler> time_of_day_scheduler_;
  TestChromeWebUIControllerFactory test_chrome_webui_controller_factory_;
  TestPersonalizationAppWebUIProvider test_webui_provider_;
  content::ScopedWebUIControllerFactoryRegistration
      scoped_controller_factory_registration_{
          &test_chrome_webui_controller_factory_};
};

INSTANTIATE_TEST_SUITE_P(
    Times,
    PersonalizationAppTimeOfDayBrowserTest,
    testing::Values(TimeOfDayTestParams({
        .timestamps_to_test =
            {
                "2023-02-01 19:00:00 UTC",
                "2023-02-02 05:59:00.000 UTC",
                "2023-02-02 06:00:01.000 UTC",
                "2023-02-02 09:59:59.000 UTC",
                "2023-02-02 10:00:01.000 UTC",
                "2023-02-02 16:20:00.000 UTC",
                "2023-02-02 18:05:00.000 UTC",
                "2023-02-03 06:00:01.000 UTC",
                "2023-02-03 10:00:01.000 UTC",
                "2023-02-03 16:00:01.000 UTC",
            },
        .expected_schedule_checkpoints =
            {
                ScheduleCheckpoint::kSunset,
                ScheduleCheckpoint::kSunset,
                ScheduleCheckpoint::kSunset,
                ScheduleCheckpoint::kSunrise,
                ScheduleCheckpoint::kSunrise,
                ScheduleCheckpoint::kLateAfternoon,
                ScheduleCheckpoint::kSunset,
                ScheduleCheckpoint::kSunset,
                ScheduleCheckpoint::kSunrise,
                ScheduleCheckpoint::kLateAfternoon,
            },
        .expected_image_types =
            {
                backdrop::Image::IMAGE_TYPE_DARK_MODE,
                backdrop::Image::IMAGE_TYPE_DARK_MODE,
                backdrop::Image::IMAGE_TYPE_DARK_MODE,
                backdrop::Image::IMAGE_TYPE_LIGHT_MODE,
                backdrop::Image::IMAGE_TYPE_LIGHT_MODE,
                backdrop::Image::IMAGE_TYPE_LATE_AFTERNOON_MODE,
                backdrop::Image::IMAGE_TYPE_DARK_MODE,
                backdrop::Image::IMAGE_TYPE_DARK_MODE,
                backdrop::Image::IMAGE_TYPE_LIGHT_MODE,
                backdrop::Image::IMAGE_TYPE_LATE_AFTERNOON_MODE,
            },
        .geoposition = MakeGeoposition("2023-02-01 19:00:00 UTC"),
    })));

IN_PROC_BROWSER_TEST_P(PersonalizationAppTimeOfDayBrowserTest,
                       ShowsExpectedImageTypesAtCheckpoints) {
  Browser* browser;
  auto* web_contents = LaunchAppAtWallpaperSubpage(&browser);

  {
    // Select a time of day wallpaper.
    base::RunLoop loop;
    WallpaperChangedWaiter waiter(loop.QuitClosure());
    web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"personalizationTestApi.selectTimeOfDayWallpaper();",
        base::DoNothing(), content::ISOLATED_WORLD_ID_GLOBAL);
    loop.Run();
  }

  auto* wallpaper_controller = Shell::Get()->wallpaper_controller();

  ASSERT_EQ(WallpaperType::kOnline, wallpaper_controller->GetWallpaperType())
      << "Time of day wallpaper expected";
  ASSERT_EQ(wallpaper_handlers::MockBackdropImageInfoFetcher::kTimeOfDayUnitId,
            wallpaper_controller->GetActiveUserWallpaperInfo()->unit_id.value())
      << "Time of day wallpaper unit id set as wallpaper";

  std::vector<ScheduleCheckpoint> all_checkpoints({GetCurrentCheckpoint()});
  std::vector<backdrop::Image::ImageType> all_image_types = {
      FirstValidVariant(
          wallpaper_controller->GetActiveUserWallpaperInfo()->variants,
          GetCurrentCheckpoint())
          ->type};

  for (const auto& checkpoint_time : TimesToIterate()) {
    const bool checkpoint_change = FastForwardBy(checkpoint_time - Now());
    if (checkpoint_change) {
      base::RunLoop loop;
      WallpaperChangedWaiter waiter(loop.QuitClosure());
      loop.Run();
    }

    all_checkpoints.push_back(GetCurrentCheckpoint());

    auto current_wallpaper_info =
        wallpaper_controller->GetActiveUserWallpaperInfo().value();
    ASSERT_EQ(
        wallpaper_handlers::MockBackdropImageInfoFetcher::kTimeOfDayUnitId,
        current_wallpaper_info.unit_id.value())
        << "WallpaperInfo still has time of day unit_id";
    all_image_types.push_back(FirstValidVariant(current_wallpaper_info.variants,
                                                GetCurrentCheckpoint())
                                  ->type);
  }

  EXPECT_EQ(ExpectedScheduleCheckpoints(), all_checkpoints);
  EXPECT_EQ(ExpectedImageTypes(), all_image_types);
}

}  // namespace
}  // namespace ash::personalization_app
