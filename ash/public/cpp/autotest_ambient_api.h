// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AUTOTEST_AMBIENT_API_H_
#define ASH_PUBLIC_CPP_AUTOTEST_AMBIENT_API_H_

#include "ash/ash_export.h"
#include "base/callback_forward.h"
#include "base/time/time.h"

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
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AUTOTEST_AMBIENT_API_H_
