// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_AUTOTEST_AMBIENT_API_H_
#define ASH_PUBLIC_CPP_AUTOTEST_AMBIENT_API_H_

#include "ash/ash_export.h"
#include "base/callback_forward.h"

namespace ash {

// Exposes limited API for the autotest private APIs to interact with Ambient
// mode.
class ASH_EXPORT AutotestAmbientApi {
 public:
  AutotestAmbientApi();
  AutotestAmbientApi(const AutotestAmbientApi&) = delete;
  AutotestAmbientApi& operator=(const AutotestAmbientApi&) = delete;
  ~AutotestAmbientApi();

  // Wait for the photo transition animation completes |num_completions| times.
  void WaitForPhotoTransitionAnimationCompleted(int num_completions,
                                                base::OnceClosure on_complete);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_AUTOTEST_AMBIENT_API_H_
