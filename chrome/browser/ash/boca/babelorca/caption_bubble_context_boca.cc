// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/boca/babelorca/caption_bubble_context_boca.h"

#include <string>
#include <utility>

#include "ash/accessibility/caption_bubble_context_ash.h"
#include "base/functional/callback.h"

namespace ash::babelorca {
namespace {

constexpr char kBocaSessionId[] = "BocaSessionId";
}  // namespace

CaptionBubbleContextBoca::CaptionBubbleContextBoca(
    ::captions::OpenCaptionSettingsCallback callback)
    : CaptionBubbleContextAsh(std::move(callback)) {}

CaptionBubbleContextBoca::~CaptionBubbleContextBoca() = default;

const std::string CaptionBubbleContextBoca::GetSessionId() const {
  return kBocaSessionId;
}

}  // namespace ash::babelorca
