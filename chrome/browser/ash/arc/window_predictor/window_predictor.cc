// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/window_predictor/window_predictor.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/app_restore/app_launch_handler.h"

namespace arc {

// static
WindowPredictor* WindowPredictor::GetInstance() {
  static base::NoDestructor<WindowPredictor> instance;
  return instance.get();
}

WindowPredictor::WindowPredictor() {}

WindowPredictor::~WindowPredictor() = default;

void WindowPredictor::MaybeCreateAppLaunchHandler(Profile* profile) {
  DCHECK(profile);
  if (app_launch_handler_ && app_launch_handler_->profile() == profile)
    return;

  app_launch_handler_ = std::make_unique<ArcPredictorAppLaunchHandler>(profile);
}

arc::mojom::WindowInfoPtr WindowPredictor::PredictAppWindowInfo(
    const std::string& arc_app_id,
    arc::mojom::WindowInfoPtr window_info) {
  // TODO(sstan): Generate window bounds based on display info and the
  // info saved in ArcAppListPrefs.
  return window_info;
}

}  // namespace arc
