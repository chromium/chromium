// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CAMERA_AUTOZOOM_NUDGE_H_
#define ASH_SYSTEM_CAMERA_AUTOZOOM_NUDGE_H_

#include "ash/ash_export.h"
#include "ash/system/tray/system_nudge.h"
#include "ui/gfx/paint_vector_icon.h"

namespace ash {

// Implements a contextual nudge for multipaste.
class ASH_EXPORT AutozoomNudge : public SystemNudge {
 public:
  AutozoomNudge();
  AutozoomNudge(const AutozoomNudge&) = delete;
  AutozoomNudge& operator=(const AutozoomNudge&) = delete;
  ~AutozoomNudge() override;

 protected:
  // SystemNudge:
  std::unique_ptr<SystemNudgeLabel> CreateLabelView() const override;
  const gfx::VectorIcon& GetIcon() const override;
  std::u16string GetAccessibilityText() const override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_CAMERA_AUTOZOOM_NUDGE_H_
