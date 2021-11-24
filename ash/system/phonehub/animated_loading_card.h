// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_export.h"
#include "ui/views/view.h"

#ifndef ASH_SYSTEM_PHONEHUB_ANIMATED_LOADING_CARD_H_
#define ASH_SYSTEM_PHONEHUB_ANIMATED_LOADING_CARD_H_

namespace ash {
// A camera roll item card loading animation based on specs defined here:
// https://carbon.googleplex.com/cros-ux/pages/show-off/motion#d9d2fa79-1a5e-4e51-834c-273072edbd45
class ASH_EXPORT AnimatedLoadingCard : public views::View {
 public:
  AnimatedLoadingCard();
  AnimatedLoadingCard(const AnimatedLoadingCard&) = delete;
  AnimatedLoadingCard& operator=(const AnimatedLoadingCard&) = delete;
  ~AnimatedLoadingCard() override;

 protected:
  // views::View override
  void OnPaint(gfx::Canvas* canvas) override;
};
}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_ANIMATED_LOADING_CARD_H_
