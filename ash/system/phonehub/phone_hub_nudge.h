// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_NUDGE_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_NUDGE_H_

#include "ash/ash_export.h"
#include "ash/system/tray/system_nudge.h"

namespace ash {

// Implements an educational nudge to inform eligible users
// of Phone Hub.
class ASH_EXPORT PhoneHubNudge : public SystemNudge {
 public:
  explicit PhoneHubNudge(std::u16string nudge_content);
  PhoneHubNudge(const PhoneHubNudge&) = delete;
  PhoneHubNudge& operator=(const PhoneHubNudge&) = delete;
  ~PhoneHubNudge() override;

 protected:
  // SystemNudge:
  std::unique_ptr<SystemNudgeLabel> CreateLabelView() const override;
  const gfx::VectorIcon& GetIcon() const override;
  std::u16string GetAccessibilityText() const override;

 private:
  std::u16string nudge_content_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_NUDGE_H_