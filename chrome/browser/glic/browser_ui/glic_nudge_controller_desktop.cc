// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_nudge_controller_desktop.h"

#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/glic/suggestions/glic_cue_target.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/call_to_action/call_to_action_lock.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace glic {

GlicNudgeControllerDesktop::GlicNudgeControllerDesktop(
    BrowserWindowInterface* browser_window_interface,
    TabListInterface* tab_list)
    : browser_window_interface_(browser_window_interface),
      tab_list_(tab_list),
      scoped_unowned_user_data_(
          browser_window_interface->GetUnownedUserDataHost(),
          *this) {
  CHECK(tab_list_);
  tab_list_observation_.Observe(tab_list);

  const bool cue_v2_enabled =
      base::FeatureList::IsEnabled(contextual_cueing::kContextualCueingV2);
  if (cue_v2_enabled) {
    glic::GlicCueTarget::Register(*browser_window_interface);
  }

}

GlicNudgeControllerDesktop::~GlicNudgeControllerDesktop() = default;

void GlicNudgeControllerDesktop::SetTabStripDelegate(
    GlicSplitButtonDelegate* delegate) {
  tab_strip_delegate_ = delegate;
}

void GlicNudgeControllerDesktop::SetToolbarDelegate(
    GlicSplitButtonDelegate* delegate) {
  toolbar_delegate_ = delegate;
}

std::optional<std::string> GlicNudgeControllerDesktop::GetPromptSuggestion() {
  return prompt_suggestion_;
}

void GlicNudgeControllerDesktop::ClearPromptSuggestion() {
  prompt_suggestion_.reset();
}

void GlicNudgeControllerDesktop::UpdateNudgeLabel(
    content::WebContents* web_contents,
    const std::string& nudge_label,
    std::optional<std::string> prompt_suggestion,
    std::optional<GlicNudgeActivity> activity,
    GlicNudgeActivityCallback callback) {
  tabs::TabInterface* active_tab = tab_list_->GetActiveTab();
  if (!active_tab || active_tab->GetContents() != web_contents) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       GlicNudgeActivity::kNudgeNotShownWebContents));
    return;
  }

  // Empty nudge labels close the nudge, allow those to bypass the
  // CanAcquireLock check.
  if (!nudge_label.empty() && !scoped_call_to_action_lock_ &&
      !CallToActionLock::From(browser_window_interface_)->CanAcquireLock()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       GlicNudgeActivity::kNudgeNotShownWindowCallToActionUI));
    return;
  }

  GlicSplitButtonDelegate* delegate = GetActiveDelegate();

  if (activity &&
      (activity == glic::GlicNudgeActivity::
                       kNudgeIgnoredOpenedContextualTasksSidePanel ||
       activity == glic::GlicNudgeActivity::
                       kNudgeIgnoredOmniboxContextMenuInteraction) &&
      delegate && delegate->GetIsShowingGlicNudge()) {
    delegate->OnHideGlicNudgeUI();
    OnNudgeActivity(*activity);
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

  PrefService* const pref_service =
      browser_window_interface_->GetProfile()->GetPrefs();
  if (pref_service->GetBoolean(glic::prefs::kGlicPinnedToTabstrip)) {
    // TODO: The delegate is currently TabStripActionContainer, which is a view
    // and shouldn't contain browser business logic. Refactor to use a proper
    // BrowserDelegate instead.
    if (delegate) {
      if (nudge_label.empty() && delegate->GetIsShowingGlicNudge()) {
        delegate->OnHideGlicNudgeUI();
      } else {
        delegate->OnTriggerGlicNudgeUI(NudgeParams(nudge_label));
      }
    }
  }

  if (nudge_label.empty()) {
    CHECK(activity);
    OnNudgeActivity(*activity);
  } else {
    OnNudgeActivity(glic::GlicNudgeActivity::kNudgeShown);
  }
}

void GlicNudgeControllerDesktop::OnNudgeActivity(GlicNudgeActivity activity) {
  if (!nudge_activity_callback_) {
    return;
  }
  switch (activity) {
    case GlicNudgeActivity::kNudgeShown: {
      nudge_activity_callback_.Run(GlicNudgeActivity::kNudgeShown);
      if (!scoped_call_to_action_lock_) {
        scoped_call_to_action_lock_ =
            CallToActionLock::From(browser_window_interface_)->AcquireLock();
      }
      break;
    }
    case GlicNudgeActivity::kNudgeClicked:
    case GlicNudgeActivity::kNudgeDismissed:
    case GlicNudgeActivity::kNudgeIgnoredActiveTabChanged:
    case GlicNudgeActivity::kNudgeIgnoredNavigation:
    case GlicNudgeActivity::kNudgeIgnoredOpenedContextualTasksSidePanel:
    case GlicNudgeActivity::kNudgeIgnoredOmniboxContextMenuInteraction:
      nudged_tab_handle_ = tabs::TabHandle::Null();
      nudge_activity_callback_.Run(activity);
      nudge_activity_callback_.Reset();
      scoped_call_to_action_lock_.reset();
      break;
    case GlicNudgeActivity::kNudgeNotShownWebContents:
    case GlicNudgeActivity::kNudgeNotShownWindowCallToActionUI:
      nudged_tab_handle_ = tabs::TabHandle::Null();
      scoped_call_to_action_lock_.reset();
      nudge_activity_callback_.Reset();
      break;
  }
}

void GlicNudgeControllerDesktop::SetNudgeActivityCallbackForTesting() {
  nudge_activity_callback_ = base::DoNothing();
}

void GlicNudgeControllerDesktop::OnActiveTabChanged(TabListInterface& tab_list,
                                                    tabs::TabInterface* tab) {
  // Ignore active tab changes that correspond to the tab currently showing the
  // nudge. This avoids race conditions where the JNI or asynchronous tab
  // activation event is processed after the nudge was shown.
  if (!nudged_tab_handle_.Get() || tab == nudged_tab_handle_.Get()) {
    return;
  }
  GlicSplitButtonDelegate* delegate = GetActiveDelegate();
  if (delegate && delegate->GetIsShowingGlicNudge()) {
    delegate->OnHideGlicNudgeUI();
    OnNudgeActivity(glic::GlicNudgeActivity::kNudgeIgnoredActiveTabChanged);
  }
}

void GlicNudgeControllerDesktop::OnTabListDestroyed(
    TabListInterface& tab_list) {
  tab_list_observation_.Reset();
}

GlicSplitButtonDelegate* GlicNudgeControllerDesktop::GetActiveDelegate() {
  auto* vertical_tab_strip_state_controller =
      tabs::VerticalTabStripStateController::From(browser_window_interface_);

  return vertical_tab_strip_state_controller &&
                 vertical_tab_strip_state_controller
                     ->ShouldDisplayVerticalTabs()
             ? toolbar_delegate_
             : tab_strip_delegate_;
}

}  // namespace glic
