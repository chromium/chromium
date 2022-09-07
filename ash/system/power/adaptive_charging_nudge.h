// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_NUDGE_H_
#define ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_NUDGE_H_

#include "ash/ash_export.h"
#include "ash/system/tray/system_nudge.h"

namespace ash {

// Implements a contextual nudge for apdative charging.
class ASH_EXPORT AdaptiveChargingNudge : public SystemNudge {
 public:
  explicit AdaptiveChargingNudge();
  AdaptiveChargingNudge(const AdaptiveChargingNudge&) = delete;
  AdaptiveChargingNudge& operator=(const AdaptiveChargingNudge&) = delete;
  ~AdaptiveChargingNudge() override;

 private:
  // SystemNudge:
  std::unique_ptr<SystemNudgeLabel> CreateLabelView() const override;
  const gfx::VectorIcon& GetIcon() const override;
  std::u16string GetAccessibilityText() const override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_ADAPTIVE_CHARGING_NUDGE_H_
