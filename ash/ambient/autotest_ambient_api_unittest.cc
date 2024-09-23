// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/autotest_ambient_api.h"

#include <optional>
#include <string>

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/ambient_ui_settings.h"
#include "ash/ambient/test/ambient_ash_test_base.h"
#include "ash/ambient/ui/ambient_container_view.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/constants/ambient_video.h"
#include "ash/public/cpp/ambient/ambient_prefs.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_ash_web_view.h"
#include "ash/webui/personalization_app/mojom/personalization_app.mojom-shared.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/ash/components/dbus/dlcservice/fake_dlcservice_client.h"
#include "components/prefs/pref_service.h"
#include "net/base/url_util.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using ash::personalization_app::mojom::AmbientTheme;
using ::testing::NotNull;

constexpr base::TimeDelta kVideoPlaybackTimeout = base::Seconds(10);

class AutotestAmbientApiTest : public AmbientAshTestBase {
 protected:
  void SetUp() override {
    AmbientAshTestBase::SetUp();
    ash_test_helper()->dlc_service_client()->set_install_root_path(
        "/test/dlc/root/path");
  }

  void ScheduleVideoPlaybackStarted(base::TimeDelta delay, bool success) {
    auto signal_video_playback_started = [this, success]() {
      TestAshWebView* web_view = static_cast<TestAshWebView*>(
          GetContainerView()->GetViewByID(kAmbientVideoWebView));
      ASSERT_THAT(web_view, NotNull());
      ASSERT_FALSE(web_view->GetVisibleURL().is_empty());
      base::Value::Dict url_fragment_dict;
      url_fragment_dict.Set("playback_started", success);
      std::optional<std::string> url_fragment =
          base::WriteJson(url_fragment_dict);
      CHECK(url_fragment);
      web_view->Navigate(
          net::AppendOrReplaceRef(web_view->GetVisibleURL(), *url_fragment));
    };

    // Simulate video playback starting half-way to the timeout.
    task_environment()->GetMainThreadTaskRunner()->PostDelayedTask(
        FROM_HERE, base::BindLambdaForTesting(signal_video_playback_started),
        delay);
  }

  base::test::TestFuture<void> completion_;
  base::test::TestFuture<void> timeout_;
  base::test::TestFuture<std::string> error_;
};

TEST_F(AutotestAmbientApiTest,
       ShouldSuccessfullyWaitForPhotoTransitionAnimation) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  prefs->SetInteger(ambient::prefs::kAmbientModePhotoRefreshIntervalSeconds, 2);

  SetAmbientTheme(AmbientTheme::kSlideshow);
  SetAmbientShownAndWaitForWidgets();

  // Wait for 10 photo transition animation to complete.
  AutotestAmbientApi test_api;
  test_api.WaitForPhotoTransitionAnimationCompleted(
      /*num_completions=*/10, /*timeout=*/base::Seconds(30),
      /*on_complete=*/completion_.GetCallback(),
      /*on_timeout=*/timeout_.GetCallback());
  task_environment()->FastForwardBy(base::Seconds(30));
  EXPECT_TRUE(completion_.IsReady());
  EXPECT_FALSE(timeout_.IsReady());
}

TEST_F(AutotestAmbientApiTest,
       ShouldCallTimeoutCallbackIfNotEnoughPhotoTransitions) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetPrimaryUserPrefService();
  prefs->SetInteger(ambient::prefs::kAmbientModePhotoRefreshIntervalSeconds, 2);

  SetAmbientTheme(AmbientTheme::kSlideshow);
  SetAmbientShownAndWaitForWidgets();

  AutotestAmbientApi test_api;
  test_api.WaitForPhotoTransitionAnimationCompleted(
      /*num_completions=*/10, /*timeout=*/base::Seconds(5),
      /*on_complete=*/completion_.GetCallback(),
      /*on_timeout=*/timeout_.GetCallback());
  task_environment()->FastForwardBy(base::Seconds(5));
  EXPECT_FALSE(completion_.IsReady());
  EXPECT_TRUE(timeout_.IsReady());
}

TEST_F(AutotestAmbientApiTest, ShouldSuccessfullyWaitForVideoStarted) {
  SetAmbientUiSettings(
      AmbientUiSettings(AmbientTheme::kVideo, kDefaultAmbientVideo));
  SetAmbientShownAndWaitForWidgets();

  // Simulate video playback starting half-way to the timeout.
  ScheduleVideoPlaybackStarted(kVideoPlaybackTimeout / 2, /*success=*/true);

  AutotestAmbientApi test_api;
  test_api.WaitForVideoToStart(kVideoPlaybackTimeout,
                               /*on_complete=*/completion_.GetCallback(),
                               /*on_error=*/error_.GetCallback(),
                               task_environment()->GetMockTickClock());
  task_environment()->FastForwardBy(kVideoPlaybackTimeout);
  EXPECT_TRUE(completion_.IsReady());
  EXPECT_FALSE(error_.IsReady());
}

TEST_F(AutotestAmbientApiTest, ShouldCallErrorCallbackIfVideoPlaybackTimedOut) {
  SetAmbientUiSettings(
      AmbientUiSettings(AmbientTheme::kVideo, kDefaultAmbientVideo));
  SetAmbientShownAndWaitForWidgets();

  AutotestAmbientApi test_api;
  test_api.WaitForVideoToStart(kVideoPlaybackTimeout,
                               /*on_complete=*/completion_.GetCallback(),
                               /*on_error=*/error_.GetCallback(),
                               task_environment()->GetMockTickClock());
  task_environment()->FastForwardBy(kVideoPlaybackTimeout);
  EXPECT_FALSE(completion_.IsReady());
  EXPECT_TRUE(error_.IsReady());
}

TEST_F(AutotestAmbientApiTest, ShouldCallErrorCallbackIfVideoPlaybackFailed) {
  SetAmbientUiSettings(
      AmbientUiSettings(AmbientTheme::kVideo, kDefaultAmbientVideo));
  SetAmbientShownAndWaitForWidgets();

  // Simulate immediate video playback failure.
  ScheduleVideoPlaybackStarted(base::TimeDelta(), /*success=*/false);

  AutotestAmbientApi test_api;
  test_api.WaitForVideoToStart(kVideoPlaybackTimeout,
                               /*on_complete=*/completion_.GetCallback(),
                               /*on_error=*/error_.GetCallback(),
                               task_environment()->GetMockTickClock());
  task_environment()->FastForwardBy(kVideoPlaybackTimeout);
  EXPECT_FALSE(completion_.IsReady());
  EXPECT_TRUE(error_.IsReady());
}

}  // namespace ash
