// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_nudge_controller_android.h"

#include "base/notimplemented.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_delegate.h"
#include "chrome/browser/tab_list/tab_list_interface.h"

namespace glic {

GlicNudgeControllerAndroid::GlicNudgeControllerAndroid(
    TabListInterface* tab_list)
    : tab_list_(tab_list) {
  if (tab_list_) {
    tab_list_observation_.Observe(tab_list_);
  }
}
GlicNudgeControllerAndroid::~GlicNudgeControllerAndroid() = default;

void GlicNudgeControllerAndroid::SetTabStripDelegate(
    GlicNudgeDelegate* delegate) {
  tab_strip_delegate_ = delegate;
}

void GlicNudgeControllerAndroid::SetToolbarDelegate(
    GlicNudgeDelegate* delegate) {
  NOTIMPLEMENTED() << "No toolbar glic nudge on Android currently.";
}

void GlicNudgeControllerAndroid::UpdateNudgeLabel(
    content::WebContents* web_contents,
    const std::string& nudge_label,
    std::optional<std::string> prompt_suggestion,
    const std::string& anchored_message_text,
    std::optional<GlicNudgeActivity> activity,
    GlicNudgeActivityCallback callback) {
  if (auto* active_tab = tab_list_->GetActiveTab()) {
    if (active_tab->GetContents() != web_contents) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(std::move(callback),
                         GlicNudgeActivity::kNudgeNotShownWebContents));
      return;
    }
  }

  nudge_activity_callback_ = callback;
  prompt_suggestion_ = prompt_suggestion;

  GlicNudgeDelegate* delegate = tab_strip_delegate_;

  if (delegate) {
    if (nudge_label.empty() && delegate->GetIsShowingGlicNudge()) {
      delegate->OnHideGlicNudgeUI();
    } else if (!nudge_label.empty()) {
      delegate->OnTriggerGlicNudgeUI(NudgeParams(nudge_label));
    }
  }

  if (nudge_label.empty()) {
    CHECK(activity);
    OnNudgeActivity(*activity);
  } else {
    OnNudgeActivity(glic::GlicNudgeActivity::kNudgeShown);
  }
}

void GlicNudgeControllerAndroid::OnNudgeActivity(GlicNudgeActivity activity) {
  if (!nudge_activity_callback_) {
    return;
  }
  switch (activity) {
    case GlicNudgeActivity::kNudgeShown:
      nudge_activity_callback_.Run(GlicNudgeActivity::kNudgeShown);
      break;
    case GlicNudgeActivity::kNudgeClicked:
    case GlicNudgeActivity::kNudgeDismissed:
    case GlicNudgeActivity::kNudgeIgnoredActiveTabChanged:
    case GlicNudgeActivity::kNudgeIgnoredNavigation:
    case GlicNudgeActivity::kNudgeIgnoredOpenedContextualTasksSidePanel:
    case GlicNudgeActivity::kNudgeIgnoredOmniboxContextMenuInteraction:
      nudge_activity_callback_.Run(activity);
      nudge_activity_callback_.Reset();
      break;
    case GlicNudgeActivity::kNudgeNotShownWebContents:
    case GlicNudgeActivity::kNudgeNotShownWindowCallToActionUI:
      nudge_activity_callback_.Reset();
      break;
  }
}

void GlicNudgeControllerAndroid::OnActiveTabChanged(TabListInterface& tab_list,
                                                    tabs::TabInterface* tab) {
  GlicNudgeDelegate* delegate = tab_strip_delegate_;
  if (delegate && delegate->GetIsShowingGlicNudge()) {
    delegate->OnHideGlicNudgeUI();
    OnNudgeActivity(glic::GlicNudgeActivity::kNudgeIgnoredActiveTabChanged);
  }
}

std::optional<std::string> GlicNudgeControllerAndroid::GetPromptSuggestion() {
  return prompt_suggestion_;
}

void GlicNudgeControllerAndroid::ClearPromptSuggestion() {
  prompt_suggestion_.reset();
}

}  // namespace glic
