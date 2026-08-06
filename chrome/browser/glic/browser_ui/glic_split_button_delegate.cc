// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"

#include <utility>

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

GlicSplitButtonDelegate::~GlicSplitButtonDelegate() = default;

void GlicSplitButtonDelegate::OnTriggerGlicNudgeUI(NudgeParams params) {}
void GlicSplitButtonDelegate::OnHideGlicNudgeUI() {}
bool GlicSplitButtonDelegate::GetIsShowingGlicNudge() {
  return false;
}
void GlicSplitButtonDelegate::ShowGlicActorTaskIcon() {}
void GlicSplitButtonDelegate::HideGlicActorTaskIcon() {}
bool GlicSplitButtonDelegate::GetIsShowingGlicActorTaskIconNudge() {
  return false;
}
void GlicSplitButtonDelegate::SetGlicActorNudgeLabel(
    const std::u16string& nudge_label) {}
void GlicSplitButtonDelegate::TriggerGlicActorNudge(
    const std::u16string& nudge_text) {}
void GlicSplitButtonDelegate::SetGlicActorNudgePressedState(bool pressed) {}
void GlicSplitButtonDelegate::ShowActorTaskListBubble() {}
void GlicSplitButtonDelegate::CloseActorTaskListBubble() {}
bool GlicSplitButtonDelegate::IsActorTaskListBubbleShowing() {
  return false;
}
void GlicSplitButtonDelegate::SetGlicShowState(bool show) {}
void GlicSplitButtonDelegate::SetGlicPanelIsOpen(bool open) {}

}  // namespace glic
