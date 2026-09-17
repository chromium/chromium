// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_split_button_view_delegate.h"

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

GlicSplitButtonViewDelegate::~GlicSplitButtonViewDelegate() = default;

void GlicSplitButtonViewDelegate::OnTriggerGlicNudgeUI(NudgeParams params) {}
void GlicSplitButtonViewDelegate::OnHideGlicNudgeUI() {}
bool GlicSplitButtonViewDelegate::GetIsShowingGlicNudge() {
  return false;
}
void GlicSplitButtonViewDelegate::ShowGlicActorTaskIcon() {}
void GlicSplitButtonViewDelegate::HideGlicActorTaskIcon() {}
bool GlicSplitButtonViewDelegate::GetIsShowingGlicActorTaskIconNudge() {
  return false;
}
void GlicSplitButtonViewDelegate::SetGlicActorNudgeLabel(
    const std::u16string& nudge_label) {}
void GlicSplitButtonViewDelegate::TriggerGlicActorNudge(
    const std::u16string& nudge_label) {}
void GlicSplitButtonViewDelegate::SetGlicActorNudgePressedState(bool pressed) {}
void GlicSplitButtonViewDelegate::ShowActorTaskListBubble() {}
void GlicSplitButtonViewDelegate::CloseActorTaskListBubble() {}
bool GlicSplitButtonViewDelegate::IsActorTaskListBubbleShowing() {
  return false;
}
void GlicSplitButtonViewDelegate::SetGlicShowState(bool show) {}
void GlicSplitButtonViewDelegate::SetGlicPanelIsOpen(bool open) {}

}  // namespace glic
