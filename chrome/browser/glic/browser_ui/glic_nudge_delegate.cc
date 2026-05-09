// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_nudge_delegate.h"

namespace glic {

NudgeParams::~NudgeParams() = default;
NudgeParams::NudgeParams(NudgeParams&&) = default;
NudgeParams& NudgeParams::operator=(NudgeParams&&) = default;

NudgeParams::NudgeParams(std::string label)
    : NudgeParams(std::move(label), {}, {}) {}
NudgeParams::NudgeParams(std::string label,
                         std::string anchored_message_text,
                         std::optional<std::string> prompt_suggestion)
    : label(std::move(label)),
      anchored_message_text(std::move(anchored_message_text)),
      prompt_suggestion(std::move(prompt_suggestion)) {}

GlicNudgeDelegate::~GlicNudgeDelegate() = default;

}  // namespace glic
