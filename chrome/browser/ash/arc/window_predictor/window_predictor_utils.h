// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_WINDOW_PREDICTOR_WINDOW_PREDICTOR_UTILS_H_
#define CHROME_BROWSER_ASH_ARC_WINDOW_PREDICTOR_WINDOW_PREDICTOR_UTILS_H_

#include "ash/components/arc/metrics/arc_metrics_constants.h"
#include "ash/components/arc/mojom/app.mojom.h"

class Profile;

namespace arc {

// Create ARC app ghost window and add the corresponding to the launching list,
// it will be launched after ARC ready.
bool LaunchArcAppWithGhostWindow(Profile* profile,
                                 const std::string& arc_app_id,
                                 int event_flags,
                                 arc::UserInteractionType user_interaction_type,
                                 arc::mojom::WindowInfoPtr window_info);

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_WINDOW_PREDICTOR_WINDOW_PREDICTOR_UTILS_H_
