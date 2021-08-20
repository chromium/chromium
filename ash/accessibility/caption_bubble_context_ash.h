// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_CAPTION_BUBBLE_CONTEXT_ASH_H_
#define ASH_ACCESSIBILITY_CAPTION_BUBBLE_CONTEXT_ASH_H_

#include "components/live_caption/caption_bubble_context.h"

namespace ash {
namespace captions {

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble Context for Ash
//
//  The implementation of the Caption Bubble Context for Ash.
//
class CaptionBubbleContextAsh : public ::captions::CaptionBubbleContext {
 public:
  CaptionBubbleContextAsh();
  ~CaptionBubbleContextAsh() override;
  CaptionBubbleContextAsh(const CaptionBubbleContextAsh&) = delete;
  CaptionBubbleContextAsh& operator=(const CaptionBubbleContextAsh&) = delete;

  // ::captions::CaptionBubbleContext:
  absl::optional<gfx::Rect> GetBounds() const override;
  void Activate() override {}
  bool IsActivatable() const override;
};

}  // namespace captions
}  // namespace ash

#endif  // ASH_ACCESSIBILITY_CAPTION_BUBBLE_CONTEXT_ASH_H_
