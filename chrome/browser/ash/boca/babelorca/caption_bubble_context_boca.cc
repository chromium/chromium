// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/babelorca/caption_bubble_context_boca.h"

#include <string>
#include <utility>

#include "ash/accessibility/caption_bubble_context_ash.h"
#include "base/functional/callback.h"
#include "components/live_caption/views/caption_bubble_model.h"

namespace ash::babelorca {
CaptionBubbleContextBoca::CaptionBubbleContextBoca(
    ::captions::OpenCaptionSettingsCallback callback,
    bool translation_enabled)
    : CaptionBubbleContextAsh(std::move(callback)),
      translation_enabled_(translation_enabled) {}

CaptionBubbleContextBoca::~CaptionBubbleContextBoca() = default;

const std::string CaptionBubbleContextBoca::GetSessionId() const {
  return translation_enabled_
             ? ::captions::CaptionBubbleModel::kBocaWithTranslationSessionId
             : ::captions::CaptionBubbleModel::kBocaNoTranslationSessionId;
}

}  // namespace ash::babelorca
