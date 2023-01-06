// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/autotest_ambient_api.h"

#include "ash/ambient/ambient_controller.h"
#include "ash/ambient/model/ambient_photo_config.h"
#include "ash/ambient/ui/ambient_view_delegate.h"
#include "ash/public/cpp/ambient/ambient_backend_controller.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

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

}  // namespace ash
