// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"

#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_delegate.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/glic/suggestions/glic_cue_target.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/call_to_action/call_to_action_lock.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/glic/browser_ui/anchored_nudge_controller.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#endif

namespace glic {

GlicNudgeController::GlicNudgeController(
    BrowserWindowInterface* browser_window_interface,
    TabListInterface* tab_list)
    : browser_window_interface_(browser_window_interface), tab_list_(tab_list) {
  CHECK(tab_list_);
  tab_list_observation_.Observe(tab_list);

  const bool cue_v2_enabled =
      base::FeatureList::IsEnabled(contextual_cueing::kContextualCueingV2);
  if (cue_v2_enabled) {
    glic::GlicCueTarget::Register(*browser_window_interface);
  }

#if !BUILDFLAG(IS_ANDROID)
  if (!cue_v2_enabled && base::FeatureList::IsEnabled(kUseAnchoredMessage)) {
    anchored_nudge_controller_ =
        std::make_unique<AnchoredNudgeController>(*browser_window_interface);
  }
#endif
}

GlicNudgeController::~GlicNudgeController() = default;

void GlicNudgeController::UpdateNudgeLabel(
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

  GlicNudgeDelegate* delegate = GetActiveDelegate();

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

  PrefService* const pref_service =
      browser_window_interface_->GetProfile()->GetPrefs();
  if (pref_service->GetBoolean(glic::prefs::kGlicPinnedToTabstrip)) {
    // TODO: The delegate is currently TabStripActionContainer, which is a view
    // and shouldn't contain browser business logic. Refactor to use a proper
    // BrowserDelegate instead.
    if (delegate) {
      if (nudge_label.empty() && delegate->GetIsShowingGlicNudge()) {
        delegate->OnHideGlicNudgeUI();
      } else if (base::FeatureList::IsEnabled(kUseAnchoredMessage) &&
                 !anchored_message_text.empty()) {
#if !BUILDFLAG(IS_ANDROID)
        anchored_nudge_controller_->OnTriggerGlicNudgeUI(NudgeParams(
            nudge_label, anchored_message_text, std::move(prompt_suggestion)));
#endif
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

void GlicNudgeController::OnNudgeActivity(GlicNudgeActivity activity) {
  if (!nudge_activity_callback_) {
    return;
  }
  switch (activity) {
    case GlicNudgeActivity::kNudgeShown: {
      // UpdateNudgeLabel can be called multiple times to update the text of an
      // existing nudge. We run the logic below to ensure the new callback is
      // invoked. The lock is only acquired if not already held.
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
      nudge_activity_callback_.Run(activity);
      nudge_activity_callback_.Reset();
      scoped_call_to_action_lock_.reset();

      break;
    case GlicNudgeActivity::kNudgeNotShownWebContents:
    case GlicNudgeActivity::kNudgeNotShownWindowCallToActionUI:
      scoped_call_to_action_lock_.reset();
      nudge_activity_callback_.Reset();
      break;
  }
}

void GlicNudgeController::SetNudgeActivityCallbackForTesting() {
  nudge_activity_callback_ = base::DoNothing();
}

void GlicNudgeController::OnActiveTabChanged(TabListInterface& tab_list,
                                             tabs::TabInterface* tab) {
  GlicNudgeDelegate* delegate = GetActiveDelegate();
  if (delegate && delegate->GetIsShowingGlicNudge()) {
    delegate->OnHideGlicNudgeUI();
    OnNudgeActivity(glic::GlicNudgeActivity::kNudgeIgnoredActiveTabChanged);
  }
}

GlicNudgeDelegate* GlicNudgeController::GetActiveDelegate() {
#if !BUILDFLAG(IS_ANDROID)
  if (anchored_nudge_controller_) {
    return anchored_nudge_controller_.get();
  }

  auto* vertical_tab_strip_state_controller =
      tabs::VerticalTabStripStateController::From(browser_window_interface_);

  return vertical_tab_strip_state_controller &&
                 vertical_tab_strip_state_controller
                     ->ShouldDisplayVerticalTabs()
             ? toolbar_delegate_
             : tab_strip_delegate_;
#else
  return tab_strip_delegate_;
#endif
}

}  // namespace glic
