// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_STYLE_DARK_LIGHT_MODE_NUDGE_H_
#define ASH_STYLE_DARK_LIGHT_MODE_NUDGE_H_

#include "ash/system/tray/system_nudge.h"

namespace ash {

// Implements an educational nudge for dark light mode.
class DarkLightModeNudge : public SystemNudge {
 public:
  DarkLightModeNudge();
  DarkLightModeNudge(const DarkLightModeNudge&) = delete;
  DarkLightModeNudge& operator=(const DarkLightModeNudge&) = delete;
  ~DarkLightModeNudge() override;

 protected:
  // SystemNudge:
  std::unique_ptr<SystemNudgeLabel> CreateLabelView() const override;
  const gfx::VectorIcon& GetIcon() const override;
  std::u16string GetAccessibilityText() const override;
};

}  // namespace ash

#endif  // ASH_STYLE_DARK_LIGHT_MODE_NUDGE_H_
