// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONSTANTS_AMBIENT_VIDEO_H_
#define ASH_CONSTANTS_AMBIENT_VIDEO_H_

#include <string_view>

#include "base/component_export.h"

namespace ash {

// Only applies when |AmbientTheme::kVideo| is active.
//
// Each corresponds to a video in ambient mode that can be selected by the user.
// The videos get played on loop at full screen. They are static and
// Google-owned.
//
// These values are persisted in user pref storage and logs, so they should
// never be renumbered or reused.
enum class AmbientVideo {
  kNewMexico = 0,
  kClouds = 1,
  kMaxValue = kClouds,
};

// Before the user explicitly selects anything, the hub automatically selects
// this default for the user when the video theme is active.
inline constexpr AmbientVideo kDefaultAmbientVideo = AmbientVideo::kNewMexico;

// The returned string_view is guaranteed to be null-terminated and point to
// memory valid for the lifetime of the program.
COMPONENT_EXPORT(ASH_CONSTANTS)
std::string_view ToString(AmbientVideo video);

}  // namespace ash

#endif  // ASH_CONSTANTS_AMBIENT_VIDEO_H_
