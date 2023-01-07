// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/model/ambient_photo_config.h"

namespace ash {

AmbientPhotoConfig::AmbientPhotoConfig() = default;

AmbientPhotoConfig::AmbientPhotoConfig(const AmbientPhotoConfig& other) =
    default;

AmbientPhotoConfig& AmbientPhotoConfig::operator=(
    const AmbientPhotoConfig& other) = default;

AmbientPhotoConfig::~AmbientPhotoConfig() = default;

std::ostream& operator<<(std::ostream& os, AmbientPhotoConfig::Marker marker) {
  switch (marker) {
    case AmbientPhotoConfig::Marker::kUiStartRendering:
      return os << "UI_START_RENDERING";
    case AmbientPhotoConfig::Marker::kUiCycleEnded:
      return os << "UI_CYCLE_ENDED";
  }
}

}  // namespace ash
