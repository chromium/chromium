// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_CAPTION_BUBBLE_CONTEXT_ASH_H_
#define ASH_ACCESSIBILITY_CAPTION_BUBBLE_CONTEXT_ASH_H_

#include "ash/ash_export.h"
#include "components/live_caption/caption_bubble_context.h"

#include <memory>
#include <string>

namespace ash::captions {

///////////////////////////////////////////////////////////////////////////////
// Caption Bubble Context for Ash
//
//  The implementation of the Caption Bubble Context for Ash.
//
class ASH_EXPORT CaptionBubbleContextAsh
    : public ::captions::CaptionBubbleContext {
 public:
  CaptionBubbleContextAsh();
  ~CaptionBubbleContextAsh() override;
  CaptionBubbleContextAsh(const CaptionBubbleContextAsh&) = delete;
  CaptionBubbleContextAsh& operator=(const CaptionBubbleContextAsh&) = delete;

  // ::captions::CaptionBubbleContext:
  absl::optional<gfx::Rect> GetBounds() const override;
  const std::string GetSessionId() const override;
  void Activate() override {}
  bool IsActivatable() const override;
  std::unique_ptr<::captions::CaptionBubbleSessionObserver>
  GetCaptionBubbleSessionObserver() override;
};

}  // namespace ash::captions

#endif  // ASH_ACCESSIBILITY_CAPTION_BUBBLE_CONTEXT_ASH_H_
