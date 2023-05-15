// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_DICTATION_NUDGE_H_
#define ASH_ACCESSIBILITY_DICTATION_NUDGE_H_

#include <string>

#include "ash/ash_export.h"
#include "ash/system/tray/system_nudge.h"
#include "base/memory/raw_ptr.h"

namespace ash {

class DictationNudgeController;
class DictationNudgeControllerTest;

// Implements a contextual nudge for Dictation informing the user
// that their Dictation language now works offline.
class ASH_EXPORT DictationNudge : public SystemNudge {
 public:
  explicit DictationNudge(DictationNudgeController* controller);
  DictationNudge(const DictationNudge&) = delete;
  DictationNudge& operator=(const DictationNudge&) = delete;
  ~DictationNudge() override;

 protected:
  // SystemNudge:
  std::unique_ptr<SystemNudgeLabel> CreateLabelView() const override;
  const gfx::VectorIcon& GetIcon() const override;
  std::u16string GetAccessibilityText() const override;

 private:
  friend class DictationNudgeControllerTest;

  // Unowned. The DictationNudgeController owns |this|.
  const raw_ptr<const DictationNudgeController, ExperimentalAsh> controller_;
};

}  // namespace ash

#endif  // ASH_ACCESSIBILITY_DICTATION_NUDGE_H_
