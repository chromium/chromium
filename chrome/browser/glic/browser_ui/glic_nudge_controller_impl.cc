// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_nudge_controller_impl.h"

#include "base/functional/bind.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_controller.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/suggestions/contextual_cueing_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/call_to_action/call_to_action_lock.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

namespace glic {

GlicNudgeControllerImpl::GlicNudgeControllerImpl(
    BrowserWindowInterface* browser_window_interface,
    GlicSplitButtonController* split_button_controller)
    : browser_window_interface_(browser_window_interface),
      split_button_controller_(split_button_controller),
      scoped_unowned_user_data_(
          browser_window_interface->GetUnownedUserDataHost(),
          *this) {
  if (TabListInterface* tab_list = GetTabList()) {
    tab_list_observation_.Observe(tab_list);
  }
}

GlicNudgeControllerImpl::~GlicNudgeControllerImpl() = default;

// TODO(crbug.com/511309088): Remove and have callers do this directly on the
// split button controller.
void GlicNudgeControllerImpl::SetHorizontalTabsDelegate(
    GlicSplitButtonDelegate* delegate) {
  split_button_controller_->SetHorizontalTabsDelegate(delegate);
}

void GlicNudgeControllerImpl::SetVerticalTabsDelegate(
    GlicSplitButtonDelegate* delegate) {
  split_button_controller_->SetVerticalTabsDelegate(delegate);
}

std::optional<std::string> GlicNudgeControllerImpl::GetPromptSuggestion() {
  return prompt_suggestion_;
}

void GlicNudgeControllerImpl::ClearPromptSuggestion() {
  prompt_suggestion_.reset();
}

void GlicNudgeControllerImpl::UpdateNudgeLabel(
    content::WebContents* web_contents,
    const std::string& nudge_label,
    std::optional<std::string> prompt_suggestion,
    std::optional<GlicNudgeActivity> activity,
    GlicNudgeActivityCallback callback) {
  tabs::TabInterface* active_tab = GetTabList()->GetActiveTab();
  if (!active_tab || active_tab->GetContents() != web_contents) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       GlicNudgeActivity::kNudgeNotShownWebContents));
    return;
  }

  // Empty nudge labels close the nudge, allow those to bypass the
  // CanAcquireLock check.
  // TODO(crbug.com/484037810): Once Android has BrowserWindowFeatures, this
  // shouldn't be nullable.
  CallToActionLock* call_to_action_lock =
      CallToActionLock::From(browser_window_interface_);
  if (!nudge_label.empty() && !scoped_call_to_action_lock_ &&
      call_to_action_lock && !call_to_action_lock->CanAcquireLock()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       GlicNudgeActivity::kNudgeNotShownWindowCallToActionUI));
    return;
  }

  if (activity &&
      (activity == glic::GlicNudgeActivity::
                       kNudgeIgnoredOpenedContextualTasksSidePanel ||
       activity == glic::GlicNudgeActivity::
                       kNudgeIgnoredOmniboxContextMenuInteraction)) {
    HideNudge(*activity);
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
    split_button_controller_->CallOnBoth(base::BindRepeating(
        [](const std::string& nudge_label, GlicSplitButtonDelegate& delegate) {
          if (nudge_label.empty()) {
            delegate.OnHideGlicNudgeUI();
          } else {
            delegate.OnTriggerGlicNudgeUI(NudgeParams(nudge_label));
          }
        },
        nudge_label));
  }

  if (nudge_label.empty()) {
    CHECK(activity);
    OnNudgeActivity(*activity);
  } else {
    OnNudgeActivity(glic::GlicNudgeActivity::kNudgeShown);
  }
}

void GlicNudgeControllerImpl::OnNudgeActivity(GlicNudgeActivity activity) {
  if (!nudge_activity_callback_) {
    return;
  }
  switch (activity) {
    case GlicNudgeActivity::kNudgeShown: {
      nudge_activity_callback_.Run(GlicNudgeActivity::kNudgeShown);
      if (!scoped_call_to_action_lock_) {
        // TODO(crbug.com/484037810): Once Android has BrowserWindowFeatures,
        // this shouldn't be nullable.
        if (CallToActionLock* lock =
                CallToActionLock::From(browser_window_interface_)) {
          scoped_call_to_action_lock_ = lock->AcquireLock();
        }
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

void GlicNudgeControllerImpl::SetNudgeActivityCallbackForTesting() {
  nudge_activity_callback_ = base::DoNothing();
}

void GlicNudgeControllerImpl::OnActiveTabChanged(TabListInterface& tab_list,
                                                 tabs::TabInterface* tab) {
  // Ignore active tab changes that correspond to the tab currently showing the
  // nudge. This avoids race conditions where the JNI or asynchronous tab
  // activation event is processed after the nudge was shown.
  if (!nudged_tab_handle_.Get() || tab == nudged_tab_handle_.Get()) {
    return;
  }

  HideNudge(glic::GlicNudgeActivity::kNudgeIgnoredActiveTabChanged);
}

void GlicNudgeControllerImpl::OnTabListActiveChanged(TabListInterface& tab_list,
                                                     bool is_active) {
  if (!is_active && nudged_tab_handle_.Get()) {
    HideNudge(glic::GlicNudgeActivity::kNudgeIgnoredActiveTabChanged);
  }
}

void GlicNudgeControllerImpl::HideNudge(GlicNudgeActivity activity) {
  split_button_controller_->CallOnBoth(
      base::BindRepeating([](GlicSplitButtonDelegate& delegate) {
        if (delegate.GetIsShowingGlicNudge()) {
          delegate.OnHideGlicNudgeUI();
        }
      }));
  OnNudgeActivity(activity);
}

void GlicNudgeControllerImpl::OnTabListDestroyed(TabListInterface& tab_list) {
  tab_list_observation_.Reset();
}

TabListInterface* GlicNudgeControllerImpl::GetTabList() {
  return TabListInterface::From(browser_window_interface_);
}

base::WeakPtr<GlicNudgeController> GlicNudgeControllerImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace glic
