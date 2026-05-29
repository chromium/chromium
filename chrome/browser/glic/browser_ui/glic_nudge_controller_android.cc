// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_nudge_controller_android.h"

namespace glic {

GlicNudgeControllerAndroid::GlicNudgeControllerAndroid() = default;
GlicNudgeControllerAndroid::~GlicNudgeControllerAndroid() = default;

void GlicNudgeControllerAndroid::SetTabStripDelegate(
    GlicNudgeDelegate* delegate) {
  // Stub implementation.
}

void GlicNudgeControllerAndroid::SetToolbarDelegate(
    GlicNudgeDelegate* delegate) {
  // Stub implementation.
}

void GlicNudgeControllerAndroid::UpdateNudgeLabel(
    content::WebContents* web_contents,
    const std::string& nudge_label,
    std::optional<std::string> prompt_suggestion,
    const std::string& anchored_message_text,
    std::optional<GlicNudgeActivity> activity,
    GlicNudgeActivityCallback callback) {
  // Stub implementation.
}

void GlicNudgeControllerAndroid::OnNudgeActivity(GlicNudgeActivity activity) {
  // Stub implementation.
}

std::optional<std::string> GlicNudgeControllerAndroid::GetPromptSuggestion() {
  return std::nullopt;
}

void GlicNudgeControllerAndroid::ClearPromptSuggestion() {}

}  // namespace glic
