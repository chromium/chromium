// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_nudge_controller_android.h"

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_delegate_android.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace glic {

GlicNudgeControllerAndroid::GlicNudgeControllerAndroid(
    BrowserWindowInterface* browser)
    : browser_(browser),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {
  TabListInterface* tab_list = GetTabList();
  if (tab_list) {
    tab_list_observation_.Observe(tab_list);
  }
  delegate_ = std::make_unique<GlicNudgeDelegateAndroid>(this, browser);
  SetTabStripDelegate(delegate_.get());
}

GlicNudgeControllerAndroid::~GlicNudgeControllerAndroid() = default;

void GlicNudgeControllerAndroid::SetTabStripDelegate(
    GlicSplitButtonDelegate* delegate) {
  tab_strip_delegate_ = delegate;
}

void GlicNudgeControllerAndroid::SetToolbarDelegate(
    GlicSplitButtonDelegate* delegate) {
  NOTIMPLEMENTED() << "No toolbar glic nudge on Android currently.";
}

void GlicNudgeControllerAndroid::UpdateNudgeLabel(
    content::WebContents* web_contents,
    const std::string& nudge_label,
    std::optional<std::string> prompt_suggestion,
    std::optional<GlicNudgeActivity> activity,
    GlicNudgeActivityCallback callback) {
  TabListInterface* tab_list = GetTabList();
  tabs::TabInterface* active_tab =
      tab_list ? tab_list->GetActiveTab() : nullptr;
  if (!active_tab || active_tab->GetContents() != web_contents) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       GlicNudgeActivity::kNudgeNotShownWebContents));
    return;
  }
  nudge_activity_callback_ = callback;
  prompt_suggestion_ = prompt_suggestion;
  if (!nudge_label.empty()) {
    CHECK(active_tab);
    nudged_tab_handle_ = active_tab->GetHandle();
  } else {
    nudged_tab_handle_ = tabs::TabHandle::Null();
  }

  GlicSplitButtonDelegate* delegate = tab_strip_delegate_;

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
      nudged_tab_handle_ = tabs::TabHandle::Null();
      nudge_activity_callback_.Run(activity);
      nudge_activity_callback_.Reset();
      break;
    case GlicNudgeActivity::kNudgeNotShownWebContents:
    case GlicNudgeActivity::kNudgeNotShownWindowCallToActionUI:
      nudged_tab_handle_ = tabs::TabHandle::Null();
      nudge_activity_callback_.Reset();
      break;
  }
}

void GlicNudgeControllerAndroid::OnActiveTabChanged(TabListInterface& tab_list,
                                                    tabs::TabInterface* tab) {
  // Ignore active tab changes that correspond to the tab currently showing the
  // nudge. This avoids race conditions where the JNI or asynchronous tab
  // activation event is processed after the nudge was shown.
  if (!nudged_tab_handle_.Get() || tab == nudged_tab_handle_.Get()) {
    return;
  }
  GlicSplitButtonDelegate* delegate = tab_strip_delegate_;
  if (delegate && delegate->GetIsShowingGlicNudge()) {
    delegate->OnHideGlicNudgeUI();
    OnNudgeActivity(glic::GlicNudgeActivity::kNudgeIgnoredActiveTabChanged);
  }
}

void GlicNudgeControllerAndroid::OnTabListDestroyed(
    TabListInterface& tab_list) {
  tab_list_observation_.Reset();
}

std::optional<std::string> GlicNudgeControllerAndroid::GetPromptSuggestion() {
  return prompt_suggestion_;
}

void GlicNudgeControllerAndroid::ClearPromptSuggestion() {
  prompt_suggestion_.reset();
}

TabListInterface* GlicNudgeControllerAndroid::GetTabList() {
  return browser_ ? TabListInterface::From(browser_) : nullptr;
}

}  // namespace glic
