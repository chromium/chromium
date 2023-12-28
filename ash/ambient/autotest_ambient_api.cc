// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/autotest_ambient_api.h"

#include <utility>

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/metrics/ambient_metrics.h"
#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/ambient/ui/ambient_view_ids.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/public/cpp/ash_web_view.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

class PhotoTransitionAnimationObserver : public AmbientViewDelegateObserver {
 public:
  PhotoTransitionAnimationObserver(int num_completions,
                                   base::TimeDelta timeout,
                                   base::OnceClosure on_complete,
                                   base::OnceClosure on_timeout)
      : num_completions_(num_completions),
        on_complete_(std::move(on_complete)),
        on_timeout_(std::move(on_timeout)) {
    DCHECK_GT(num_completions, 0);
    DCHECK_GT(timeout, base::TimeDelta());
    DCHECK(on_complete_);
    DCHECK(on_timeout_);

    // |base::Unretained| is safe here because this timer will be abandoned in
    // the destructor.
    timer_.Start(FROM_HERE, timeout,
                 base::BindOnce(&PhotoTransitionAnimationObserver::OnTimeout,
                                base::Unretained(this)));

    scoped_observation_.Observe(
        Shell::Get()->ambient_controller()->ambient_view_delegate());
  }

  PhotoTransitionAnimationObserver(const PhotoTransitionAnimationObserver&) =
      delete;

  PhotoTransitionAnimationObserver& operator=(
      const PhotoTransitionAnimationObserver&) = delete;

  ~PhotoTransitionAnimationObserver() override = default;

  // AmbientViewDelegateObserver:
  void OnMarkerHit(AmbientPhotoConfig::Marker marker) override {
    if (marker != AmbientPhotoConfig::Marker::kUiCycleEnded)
      return;

    --num_completions_;
    if (num_completions_ == 0) {
      Cleanup();
      std::move(on_complete_).Run();
      delete this;
    }
  }

 private:
  void OnTimeout() {
    Cleanup();
    std::move(on_timeout_).Run();
    delete this;
  }

  void Cleanup() {
    timer_.AbandonAndStop();
    scoped_observation_.Reset();
  }

  int num_completions_;
  base::OnceClosure on_complete_;
  base::OnceClosure on_timeout_;
  base::OneShotTimer timer_;
  base::ScopedObservation<AmbientViewDelegate, AmbientViewDelegateObserver>
      scoped_observation_{this};
};

// Parameters needed to complete one call to `WaitForVideoToStart()`. They get
// forwarded through the async sequence of functions below until `on_complete`
// or `on_error` is run.
struct VideoPlaybackStatusTestParams {
  // Time at which the call to `WaitForVideoToStart()` was made.
  base::TimeTicks start_time;
  base::TimeDelta timeout;
  base::OnceClosure on_complete;
  base::OnceCallback<void(std::string)> on_error;
  // Never null. Points to default clock if a testing clock was not provided.
  raw_ptr<const base::TickClock> tick_clock;
};

void ScheduleVideoPlaybackStatusCheck(VideoPlaybackStatusTestParams params);
void OnVideoPlaybackStatusReceived(VideoPlaybackStatusTestParams params,
                                   ambient::AmbientVideoSessionStatus status);

// Polls the ambient video view once every second to see if playback has started
// successfully. The signal it uses is the "playback_started" field in the
// video view's URL that gets set when the <video> element has started playback.
// This coincidentally is the same signal that's used for metrics purposes.
void CheckVideoPlaybackStatusForTesting(VideoPlaybackStatusTestParams params) {
  CHECK(params.tick_clock);
  if (params.tick_clock->NowTicks() - params.start_time >= params.timeout) {
    std::move(params.on_error)
        .Run("Timed out waiting for ambient video playback to start.");
    return;
  }

  views::Widget* ambient_widget =
      Shell::Get()
          ->GetPrimaryRootWindowController()
          ->ambient_widget_for_testing();  // IN-TEST
  if (!ambient_widget) {
    DVLOG(4) << "Ambient session not active yet";
    ScheduleVideoPlaybackStatusCheck(std::move(params));
    return;
  }
  AshWebView* video_web_view = static_cast<AshWebView*>(
      ambient_widget->GetContentsView()->GetViewByID(kAmbientVideoWebView));
  if (!video_web_view) {
    std::move(params.on_error)
        .Run(
            "Video view missing from ambient widget. Video theme must be "
            "inactive.");
    return;
  }
  ambient::GetAmbientModeVideoSessionStatus(
      video_web_view,
      base::BindOnce(&OnVideoPlaybackStatusReceived, std::move(params)));
}

// Schedules the next polling check for whether video playback started.
void ScheduleVideoPlaybackStatusCheck(VideoPlaybackStatusTestParams params) {
  static constexpr base::TimeDelta kPollingPeriod = base::Seconds(1);
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CheckVideoPlaybackStatusForTesting, std::move(params)),
      kPollingPeriod);
}

// Either runs one of the completion callbacks or schedules the next polling
// check if the the video is still loading.
void OnVideoPlaybackStatusReceived(VideoPlaybackStatusTestParams params,
                                   ambient::AmbientVideoSessionStatus status) {
  CHECK(params.on_complete && params.on_error);
  switch (status) {
    case ambient::AmbientVideoSessionStatus::kSuccess:
      std::move(params.on_complete).Run();
      break;
    case ambient::AmbientVideoSessionStatus::kFailed:
      std::move(params.on_error)
          .Run(
              "Ambient video playback failed with a hard error in the "
              "webview.");
      break;
    case ambient::AmbientVideoSessionStatus::kLoading:
      ScheduleVideoPlaybackStatusCheck(std::move(params));
      break;
  }
}

}  // namespace

AutotestAmbientApi::AutotestAmbientApi() = default;

AutotestAmbientApi::~AutotestAmbientApi() = default;

void AutotestAmbientApi::WaitForPhotoTransitionAnimationCompleted(
    int num_completions,
    base::TimeDelta timeout,
    base::OnceClosure on_complete,
    base::OnceClosure on_timeout) {
  new PhotoTransitionAnimationObserver(
      num_completions, timeout, std::move(on_complete), std::move(on_timeout));
}

void AutotestAmbientApi::WaitForVideoToStart(
    base::TimeDelta timeout,
    base::OnceClosure on_complete,
    base::OnceCallback<void(std::string)> on_error,
    const base::TickClock* tick_clock) {
  if (!tick_clock) {
    tick_clock = base::DefaultTickClock::GetInstance();
  }
  CheckVideoPlaybackStatusForTesting({tick_clock->NowTicks(), timeout,
                                      std::move(on_complete),
                                      std::move(on_error), tick_clock});
}

}  // namespace ash
