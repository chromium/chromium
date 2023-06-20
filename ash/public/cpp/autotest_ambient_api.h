// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AUTOTEST_AMBIENT_API_H_
#define ASH_PUBLIC_CPP_AUTOTEST_AMBIENT_API_H_

#include <string>

#include "ash/ash_export.h"
#include "base/functional/callback_forward.h"
#include "base/time/time.h"

namespace base {
class TickClock;
}  // namespace base

namespace ash {

// Exposes limited API for the autotest private APIs to interact with Ambient
// mode.
class ASH_EXPORT AutotestAmbientApi {
 public:
  AutotestAmbientApi();
  AutotestAmbientApi(const AutotestAmbientApi&) = delete;
  AutotestAmbientApi& operator=(const AutotestAmbientApi&) = delete;
  ~AutotestAmbientApi();

  // Wait |timeout| for |num_completions| photo transitions to complete. Calls
  // |on_complete| if successful and |on_timeout| if |timeout| elapses before
  // enough photo transitions occur.
  void WaitForPhotoTransitionAnimationCompleted(int num_completions,
                                                base::TimeDelta timeout,
                                                base::OnceClosure on_complete,
                                                base::OnceClosure on_timeout);

  // Wait |timeout| for the ambient video to successfully start playback. Calls
  // |on_complete| if successful and |on_error| if video playback failed
  // (including if |timeout| elapses before video playback starts). |on_error|
  // is invoked with an error message describing the failure.
  void WaitForVideoToStart(base::TimeDelta timeout,
                           base::OnceClosure on_complete,
                           base::OnceCallback<void(std::string)> on_error,
                           const base::TickClock* tick_clock = nullptr);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AUTOTEST_AMBIENT_API_H_
