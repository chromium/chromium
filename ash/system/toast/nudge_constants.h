// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TOAST_NUDGE_CONSTANTS_H_
#define ASH_SYSTEM_TOAST_NUDGE_CONSTANTS_H_

#include "ash/ash_export.h"
#include "ui/gfx/geometry/insets.h"

namespace ash {

///////////////////////////////////////////////////////////////////////////////
// `SystemNudgeView` Label Constants
constexpr int kNudgeLabelWidth_TextOnlyNudge = 246;
constexpr int kNudgeLabelWidth_NudgeWithoutLeadingImage = 280;
constexpr int kNudgeLabelWidth_NudgeWithLeadingImage = 250;

inline constexpr gfx::Insets kBubbleBorderInsets = gfx::Insets(8);

}  // namespace ash

#endif  // ASH_SYSTEM_TOAST_NUDGE_CONSTANTS_H_
