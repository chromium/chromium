// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ambient_video.h"

#include <string_view>

namespace ash {

std::string_view ToString(AmbientVideo video) {
  // See the "AmbientModeThemes" <variants> tag in histograms.xml. These names
  // are currently used for metrics purposes, so they cannot be arbitrarily
  // renamed.
  switch (video) {
    case AmbientVideo::kNewMexico:
      return "NewMexico";
    case AmbientVideo::kClouds:
      return "Clouds";
  }
}

}  // namespace ash
