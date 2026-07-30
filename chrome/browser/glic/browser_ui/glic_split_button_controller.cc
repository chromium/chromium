// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_split_button_controller.h"

#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/notimplemented.h"
#include "build/build_config.h"
#include "chrome/browser/glic/browser_ui/glic_actor_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_button_controller.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller_impl.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/common/chrome_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/tabs/public/tab_interface.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#endif

namespace glic {

DEFINE_USER_DATA(GlicSplitButtonController);

// static
GlicSplitButtonController* GlicSplitButtonController::From(
    BrowserWindowInterface* browser) {
  return browser
             ? GlicSplitButtonController::Get(browser->GetUnownedUserDataHost())
             : nullptr;
}

GlicSplitButtonController::GlicSplitButtonController(
    BrowserWindowInterface* browser,
    GlicKeyedService* glic_service)
    : browser_(browser),
      glic_service_(glic_service),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {
  CHECK(glic_service);
  glic_nudge_controller_ =
      std::make_unique<GlicNudgeControllerImpl>(browser, this);

  glic_button_controller_ = std::make_unique<GlicButtonController>(
      browser->GetProfile(), *browser, this, glic_service);

  if (base::FeatureList::IsEnabled(features::kGlicActor) &&
      base::FeatureList::IsEnabled(features::kGlicActorUi) &&
      features::kGlicActorUiTaskIcon.Get() &&
      browser->GetProfile()->IsRegularProfile()) {
// TODO(crbug.com/518584352): Port this to Android.
#if !BUILDFLAG(IS_ANDROID)
    actor_task_list_bubble_controller_ =
        std::make_unique<ActorTaskListBubbleController>(browser);
#endif
    glic_actor_nudge_controller_ =
        std::make_unique<GlicActorNudgeController>(browser, this);
  }
}

GlicSplitButtonController::~GlicSplitButtonController() = default;

void GlicSplitButtonController::SetHorizontalTabsDelegate(
    GlicSplitButtonDelegate* delegate) {
  horizontal_tabs_delegate_ = delegate;
  glic_button_controller_->UpdateButton();
}

void GlicSplitButtonController::SetVerticalTabsDelegate(
    GlicSplitButtonDelegate* delegate) {
  vertical_tabs_delegate_ = delegate;
  glic_button_controller_->UpdateButton();
}

void GlicSplitButtonController::OnGlicButtonClicked() {
  auto* delegate = GetActiveDelegate();
  if (!delegate) {
    // TODO(crbug.com/511309088): This should not be reachable.
    NOTIMPLEMENTED_LOG_ONCE();
    return;
  }

// TODO(crbug.com/537848713): Maybe port to Android.
#if !BUILDFLAG(IS_ANDROID)
  // Indicate that the glic button was pressed so that we can either close the
  // IPH promo (if present) or note that it has already been used to prevent
  // unnecessarily displaying the promo.
  BrowserUserEducationInterface::From(browser_)->NotifyFeaturePromoFeatureUsed(
      feature_engagement::kIPHGlicPromoFeature,
      FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
#endif

  std::optional<std::string> prompt_suggestion =
      glic_nudge_controller_->GetPromptSuggestion();
  glic_nudge_controller_->ClearPromptSuggestion();

  const bool is_panel_showing =
      glic_service_->IsPanelShowingForBrowser(*browser_);
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser_)->GetActiveTab();
  if (!is_panel_showing && prompt_suggestion && !prompt_suggestion->empty() &&
      active_tab) {
    glic::GlicInvokeOptions options(glic::Target(*active_tab),
                                    GetInvocationSource(*delegate));
    options.prompts.push_back(std::move(*prompt_suggestion));
    glic_service_->Invoke(std::move(options));
  } else {
    glic_service_->ToggleUI(browser_,
                            /*prevent_close=*/false,
                            GetInvocationSource(*delegate));
  }

  if (delegate->GetIsShowingGlicNudge()) {
    glic_nudge_controller_->OnNudgeActivity(
        glic::GlicNudgeActivity::kNudgeClicked);
  }
}

void GlicSplitButtonController::CallOnBoth(
    base::RepeatingCallback<void(GlicSplitButtonDelegate&)> fn) {
  if (horizontal_tabs_delegate_) {
    fn.Run(*horizontal_tabs_delegate_);
  }
  if (vertical_tabs_delegate_) {
    fn.Run(*vertical_tabs_delegate_);
  }
}

GlicSplitButtonDelegate* GlicSplitButtonController::GetActiveDelegate() {
  return IsToolbarButton() ? vertical_tabs_delegate_
                           : horizontal_tabs_delegate_;
}

bool GlicSplitButtonController::IsToolbarButton() const {
#if BUILDFLAG(IS_ANDROID)
  NOTIMPLEMENTED_LOG_ONCE();
  return false;
#else
  if (base::FeatureList::IsEnabled(features::kGlicHorizontalTabToolbarButton)) {
    return true;
  }

  auto* vertical_tab_strip_state_controller =
      tabs::VerticalTabStripStateController::From(browser_);

  return vertical_tab_strip_state_controller &&
         vertical_tab_strip_state_controller->ShouldDisplayVerticalTabs();
#endif
}

mojom::InvocationSource GlicSplitButtonController::GetInvocationSource(
    GlicSplitButtonDelegate& delegate) const {
  if (delegate.GetIsShowingGlicNudge()) {
    return mojom::InvocationSource::kNudge;
  }
  return IsToolbarButton() ? mojom::InvocationSource::kToolbarButton
                           : mojom::InvocationSource::kTopChromeButton;
}

base::WeakPtr<GlicSplitButtonController>
GlicSplitButtonController::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace glic
