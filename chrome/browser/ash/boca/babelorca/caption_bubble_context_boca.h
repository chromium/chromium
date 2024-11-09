// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BOCA_BABELORCA_CAPTION_BUBBLE_CONTEXT_BOCA_H_
#define CHROME_BROWSER_ASH_BOCA_BABELORCA_CAPTION_BUBBLE_CONTEXT_BOCA_H_

#include <string>

#include "ash/accessibility/caption_bubble_context_ash.h"

namespace ash::babelorca {
class CaptionBubbleContextBoca : public ash::captions::CaptionBubbleContextAsh {
 public:
  CaptionBubbleContextBoca(::captions::OpenCaptionSettingsCallback callback,
                           bool translation_enabled);
  ~CaptionBubbleContextBoca() override;
  CaptionBubbleContextBoca(const CaptionBubbleContextBoca&) = delete;
  CaptionBubbleContextBoca& operator=(const CaptionBubbleContextBoca&) = delete;

  // ::captions::CaptionBubbleContext:
  const std::string GetSessionId() const override;

 private:
  bool translation_enabled_ = false;
};

}  // namespace ash::babelorca

#endif  // CHROME_BROWSER_ASH_BOCA_BABELORCA_CAPTION_BUBBLE_CONTEXT_BOCA_H_
