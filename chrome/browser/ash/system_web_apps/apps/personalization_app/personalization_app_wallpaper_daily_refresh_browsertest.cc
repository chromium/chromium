// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <algorithm>
#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/public/cpp/wallpaper/wallpaper_info.h"
#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/shell.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "ash/system/scheduled_feature/scheduled_feature.h"
#include "ash/wallpaper/test_wallpaper_image_downloader.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/wallpaper/wallpaper_daily_refresh_scheduler.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/test_personalization_app_webui_provider.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/ash/wallpaper_handlers/mock_wallpaper_handlers.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"

namespace ash::personalization_app {

namespace {

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

class PersonalizationAppWallpaperDailyRefreshBrowserTest
    : public SystemWebAppBrowserTestBase,
      public ScheduledFeature::Clock {
 public:
  PersonalizationAppWallpaperDailyRefreshBrowserTest() {
    base::Time start_time = base::Time::Now();
    clock_.SetNow(start_time);
    tick_clock_.SetNowTicks(base::TimeTicks() + (start_time - base::Time()));
  }

  PersonalizationAppWallpaperDailyRefreshBrowserTest(
      const PersonalizationAppWallpaperDailyRefreshBrowserTest&) = delete;
  PersonalizationAppWallpaperDailyRefreshBrowserTest& operator=(
      const PersonalizationAppWallpaperDailyRefreshBrowserTest&) = delete;

  ~PersonalizationAppWallpaperDailyRefreshBrowserTest() override = default;

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
        std::make_unique<WallpaperControllerTestApi>(wallpaper_controller());
    wallpaper_controller_test_api->SetDefaultWallpaper(
        GetAccountId(browser()->profile()));

    test_chrome_webui_controller_factory_.AddFactoryOverride(
        kChromeUIPersonalizationAppHost, &test_webui_provider_);

    auto* daily_refresh_scheduler = scheduler();
    // Disable any running timers to set a fake clock.
    daily_refresh_scheduler->SetScheduleType(ScheduleType::kNone);
    daily_refresh_scheduler->SetClockForTesting(this);
    daily_refresh_scheduler->SetScheduleType(ScheduleType::kCustom);

    WaitForTestSystemAppInstall();
  }

  void TearDownOnMainThread() override {
    SystemWebAppBrowserTestBase::TearDownOnMainThread();
  }

  content::WebContents* LaunchAppAtWallpaperSubpage(Browser** browser) {
    apps::AppLaunchParams launch_params =
        LaunchParamsForApp(ash::SystemWebAppType::PERSONALIZATION);
    launch_params.override_url =
        GURL(std::string(kChromeUIPersonalizationAppURL) +
             kWallpaperSubpageRelativeUrl);
    return LaunchApp(std::move(launch_params), browser);
  }

  WallpaperControllerImpl* wallpaper_controller() {
    return Shell::Get()->wallpaper_controller();
  }

  WallpaperDailyRefreshScheduler* scheduler() {
    return Shell::Get()
        ->wallpaper_controller()
        ->daily_refresh_scheduler_for_testing()
        .get();
  }

  // ScheduledFeature::Clock:
  base::Time Now() const override { return clock_.Now(); }

  base::TimeTicks NowTicks() const override { return tick_clock_.NowTicks(); }

  // Returns whether the total triggered a checkpoint change.
  bool FastForwardBy(base::TimeDelta total) {
    const auto advance_time = [this](base::TimeDelta advancement) {
      clock_.Advance(advancement);
      tick_clock_.Advance(advancement);
    };
    bool checkpoint_reached = false;
    auto* timer = Shell::Get()
                      ->wallpaper_controller()
                      ->daily_refresh_scheduler_for_testing()
                      ->timer();
    while (total.is_positive()) {
      base::TimeDelta advance_increment;
      if (timer->IsRunning() &&
          timer->desired_run_time() <= NowTicks() + total) {
        // Emulates the internal timer firing at its scheduled time.
        advance_increment = timer->desired_run_time() - NowTicks();
        advance_time(advance_increment);
        timer->FireNow();
        checkpoint_reached = true;
      } else {
        advance_increment = total;
        advance_time(advance_increment);
      }
      CHECK_LE(advance_increment, total);
      total -= advance_increment;
    }
    return checkpoint_reached;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::SimpleTestClock clock_;
  base::SimpleTestTickClock tick_clock_;
  TestChromeWebUIControllerFactory test_chrome_webui_controller_factory_;
  TestPersonalizationAppWebUIProvider test_webui_provider_;
  content::ScopedWebUIControllerFactoryRegistration
      scoped_controller_factory_registration_{
          &test_chrome_webui_controller_factory_};
};

IN_PROC_BROWSER_TEST_F(PersonalizationAppWallpaperDailyRefreshBrowserTest,
                       DailyWallpaperIsRefreshed) {
  Browser* browser;
  auto* web_contents = LaunchAppAtWallpaperSubpage(&browser);
  ASSERT_EQ(ScheduleType::kCustom, scheduler()->GetScheduleType());
  const char kCollectionId[] = "test_collection";
  {
    // Enables daily refresh.
    base::RunLoop loop;
    WallpaperChangedWaiter waiter(loop.QuitClosure());
    web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"personalizationTestApi.enableDailyRefresh('test_collection');",
        base::DoNothing(), content::ISOLATED_WORLD_ID_GLOBAL);
    loop.Run();
  }
  WallpaperInfo original_info =
      *wallpaper_controller()->GetActiveUserWallpaperInfo();
  ASSERT_EQ(WallpaperType::kDaily, original_info.type);
  EXPECT_EQ(kCollectionId, original_info.collection_id);
  // Fast forwards to the next day. Extra 1 minuter is used to account for
  // delays.
  const bool checkpoint_change =
      FastForwardBy(base::Hours(24) + base::Minutes(1));
  ASSERT_TRUE(checkpoint_change);
  base::RunLoop loop;
  WallpaperChangedWaiter waiter(loop.QuitClosure());
  loop.Run();
  WallpaperInfo new_info =
      *wallpaper_controller()->GetActiveUserWallpaperInfo();
  EXPECT_FALSE(original_info.MatchesSelection(new_info));
  EXPECT_EQ(original_info.collection_id, new_info.collection_id);
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppWallpaperDailyRefreshBrowserTest,
                       DailyDarkLightWallpaperIsRefreshed) {
  Browser* browser;
  auto* web_contents = LaunchAppAtWallpaperSubpage(&browser);
  ASSERT_EQ(ScheduleType::kCustom, scheduler()->GetScheduleType());
  const char kCollectionId[] = "dark_light_collection";
  auto* dark_light_controller = Shell::Get()->dark_light_mode_controller();
  dark_light_controller->SetAutoScheduleEnabled(false);
  {
    // Enables daily refresh.
    base::RunLoop loop;
    WallpaperChangedWaiter waiter(loop.QuitClosure());
    web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"personalizationTestApi.enableDailyRefresh('dark_light_collection');",
        base::DoNothing(), content::ISOLATED_WORLD_ID_GLOBAL);
    loop.Run();
  }
  WallpaperInfo original_info =
      *wallpaper_controller()->GetActiveUserWallpaperInfo();
  ASSERT_EQ(kCollectionId, original_info.collection_id);
  ASSERT_EQ(WallpaperType::kDaily, original_info.type);
  {
    // Forwards half day and expects no change in wallpaper.
    FastForwardBy(base::Hours(12));
    base::RunLoop().RunUntilIdle();
    WallpaperInfo new_info =
        *wallpaper_controller()->GetActiveUserWallpaperInfo();
    ASSERT_TRUE(original_info.MatchesAsset(new_info))
        << "No change to asset because not enough time elapsed for daily "
           "refresh."
        << " original_info=" << original_info << " new_info=" << new_info;
  }
  {
    // Toggles color mode and expects new asset from the same wallpaper.
    dark_light_controller->ToggleColorMode();
    base::RunLoop loop;
    WallpaperChangedWaiter waiter(loop.QuitClosure());
    loop.Run();
    WallpaperInfo new_info =
        *wallpaper_controller()->GetActiveUserWallpaperInfo();
    EXPECT_TRUE(original_info.MatchesSelection(new_info))
        << "Expect same wallpaper after color mode changes."
        << " original_info=" << original_info << " new_info=" << new_info;
    EXPECT_FALSE(original_info.MatchesAsset(new_info))
        << "Expect updated variant asset after color mode changes";
    EXPECT_EQ(original_info.date, new_info.date)
        << "The wallpaper's timestamp is unaffected by color mode change";
    EXPECT_EQ(original_info.collection_id, new_info.collection_id)
        << "Expect same collection";
  }
  {
    // Forwards another half day. Extra 1 minute is used to account for delays.
    // Expects a new wallpaper is set.
    const auto checkpoint_change =
        FastForwardBy(base::Hours(12) + base::Minutes(1));
    EXPECT_TRUE(checkpoint_change);
    base::RunLoop loop;
    WallpaperChangedWaiter waiter(loop.QuitClosure());
    loop.Run();
    WallpaperInfo new_info =
        *wallpaper_controller()->GetActiveUserWallpaperInfo();
    EXPECT_FALSE(original_info.MatchesSelection(new_info))
        << "Expect new daily wallpaper after 24 hours have elapsed."
        << " original_info=" << original_info << " new_info=" << new_info;
    EXPECT_EQ(original_info.collection_id, new_info.collection_id)
        << "Expect same collection";
  }
}

IN_PROC_BROWSER_TEST_F(PersonalizationAppWallpaperDailyRefreshBrowserTest,
                       DailyGooglePhotosWallpaperIsRefreshed) {
  Browser* browser;
  auto* web_contents = LaunchAppAtWallpaperSubpage(&browser);
  ASSERT_EQ(ScheduleType::kCustom, scheduler()->GetScheduleType());
  const char kAlbumId[] = "test_album";
  {
    // Enables daily refresh.
    base::RunLoop loop;
    WallpaperChangedWaiter waiter(loop.QuitClosure());
    web_contents->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        u"personalizationTestApi.enableDailyGooglePhotosRefresh('test_album');",
        base::DoNothing(), content::ISOLATED_WORLD_ID_GLOBAL);
    loop.Run();
  }
  WallpaperInfo original_info =
      *wallpaper_controller()->GetActiveUserWallpaperInfo();
  ASSERT_EQ(WallpaperType::kDailyGooglePhotos, original_info.type);
  EXPECT_EQ(kAlbumId, original_info.collection_id);
  // Fast forwards to the next day. Extra 1 minute is used to account for
  // delays.
  const bool checkpoint_change =
      FastForwardBy(base::Hours(24) + base::Minutes(1));
  ASSERT_TRUE(checkpoint_change);
  base::RunLoop loop;
  WallpaperChangedWaiter waiter(loop.QuitClosure());
  loop.Run();
  WallpaperInfo new_info =
      *wallpaper_controller()->GetActiveUserWallpaperInfo();
  EXPECT_FALSE(original_info.MatchesSelection(new_info))
      << "Expect new Google photo wallpaper after 24 hours have elapsed";
  EXPECT_EQ(original_info.collection_id, new_info.collection_id)
      << "Expect same album";
}

}  // namespace
}  // namespace ash::personalization_app
