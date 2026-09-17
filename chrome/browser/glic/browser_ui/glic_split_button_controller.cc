// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_split_button_controller.h"

#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "build/build_config.h"
#include "chrome/browser/actor/ui/task_list_bubble/actor_task_list_bubble_controller.h"
#include "chrome/browser/glic/browser_ui/gemini_split_button_delegate.h"
#include "chrome/browser/glic/browser_ui/glic_actor_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller_impl.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_view_delegate.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/common/chrome_features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"

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
    std::unique_ptr<GeminiSplitButtonDelegate> split_button_delegate)
    : browser_(browser),
      split_button_delegate_(std::move(split_button_delegate)),
      scoped_unowned_user_data_(browser->GetUnownedUserDataHost(), *this) {
  CHECK(browser_);
  CHECK(split_button_delegate_);

  glic_nudge_controller_ =
      std::make_unique<GlicNudgeControllerImpl>(browser, this);

  if (base::FeatureList::IsEnabled(features::kGlicActor) &&
      base::FeatureList::IsEnabled(features::kGlicActorUi) &&
      features::kGlicActorUiTaskIcon.Get() &&
      browser_->GetProfile()->IsRegularProfile()) {
    actor_task_list_bubble_controller_ =
        std::make_unique<ActorTaskListBubbleController>(browser, *this);
    glic_actor_nudge_controller_ =
        std::make_unique<GlicActorNudgeController>(browser, this);
  }

  subscriptions_.push_back(
      split_button_delegate_->RegisterEnabledChangedCallback(
          base::BindRepeating(&GlicSplitButtonController::UpdateButton,
                              base::Unretained(this))));
  subscriptions_.push_back(
      split_button_delegate_->RegisterPanelVisibilityChangedCallback(
          base::BindRepeating(&GlicSplitButtonController::UpdateButton,
                              base::Unretained(this))));

  // TODO(crbug.com/556353052): Abstract toolbar pinning pref and pref listener.
  if (Profile* profile = browser_->GetProfile()) {
    pref_registrar_.Init(profile->GetPrefs());
    pref_registrar_.Add(
        prefs::kGlicPinnedToTabstrip,
        base::BindRepeating(&GlicSplitButtonController::UpdateButton,
                            base::Unretained(this)));
  }

  UpdateButton();
}

GlicSplitButtonController::~GlicSplitButtonController() = default;

void GlicSplitButtonController::SetHorizontalTabsDelegate(
    GlicSplitButtonViewDelegate* delegate) {
  horizontal_tabs_delegate_ = delegate;
  UpdateButton();
}

void GlicSplitButtonController::SetVerticalTabsDelegate(
    GlicSplitButtonViewDelegate* delegate) {
  vertical_tabs_delegate_ = delegate;
  UpdateButton();
}

void GlicSplitButtonController::OnGlicButtonClicked() {
  auto* view_delegate = GetActiveViewDelegate();
  if (!view_delegate) {
    // TODO(crbug.com/511309088): This should not be reachable.
    NOTIMPLEMENTED_LOG_ONCE();
    return;
  }

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

  auto* glic_service =
      browser_->GetProfile()
          ? GlicKeyedServiceFactory::GetGlicKeyedService(browser_->GetProfile())
          : nullptr;

  // TODO(crbug.com/559194603): Move this logic to the delegate.
  if (glic_service) {
    const bool is_panel_showing =
        glic_service->IsPanelShowingForBrowser(*browser_);
    tabs::TabInterface* active_tab =
        TabListInterface::From(browser_)->GetActiveTab();
    if (!is_panel_showing && prompt_suggestion && !prompt_suggestion->empty() &&
        active_tab &&
        GlicEnabling::IsEnabledForProfile(browser_->GetProfile())) {
      glic::GlicInvokeOptions options(glic::Target(*active_tab),
                                      GetInvocationSource(*view_delegate));
      options.prompts.push_back(std::move(*prompt_suggestion));
      glic_service->Invoke(std::move(options));
    } else {
      glic_service->ToggleUI(browser_, /*prevent_close=*/false,
                             GetInvocationSource(*view_delegate));
    }
  }

  if (view_delegate->GetIsShowingGlicNudge()) {
    glic_nudge_controller_->OnNudgeActivity(
        glic::GlicNudgeActivity::kNudgeClicked);
  }
}

void GlicSplitButtonController::UpdateButton() {
  Profile* profile = browser_->GetProfile();
  if (!profile) {
    return;
  }

  // Attempt to record startup metrics when the button controller is first
  // created, no-op if startup metrics have already been measured.
  // Note that this will not record metrics for profiles that are not eligible
  // for Glic (i.e. GlicEnabling::IsProfileEligible() is false), as they will
  // never have a GlicButtonController created. Recording metrics for those
  // cases is handled by GlicProfileManager instead.
  split_button_delegate_->MaybeRecordStartupMetrics();

  // TODO(crbug.com/556353052): Abstract toolbar pinning pref.
  bool is_pinned =
      profile->GetPrefs()->GetBoolean(prefs::kGlicPinnedToTabstrip);
  bool should_show_button = split_button_delegate_->IsEnabled();

  if (!should_show_button || !is_pinned) {
    CallOnBoth(base::BindRepeating([](GlicSplitButtonViewDelegate& delegate) {
      delegate.SetGlicShowState(false);
    }));
    return;
  }

  bool is_panel_open = split_button_delegate_->IsPanelShowing();
  CallOnBoth(base::BindRepeating(
      [](bool is_panel_open, GlicSplitButtonViewDelegate& delegate) {
        delegate.SetGlicShowState(true);
        delegate.SetGlicPanelIsOpen(is_panel_open);
      },
      is_panel_open));
}

void GlicSplitButtonController::CallOnBoth(
    base::RepeatingCallback<void(GlicSplitButtonViewDelegate&)> fn) {
  if (horizontal_tabs_delegate_) {
    fn.Run(*horizontal_tabs_delegate_);
  }
  if (vertical_tabs_delegate_) {
    fn.Run(*vertical_tabs_delegate_);
  }
}

GlicSplitButtonViewDelegate*
GlicSplitButtonController::GetActiveViewDelegate() {
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
    GlicSplitButtonViewDelegate& delegate) const {
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

void GlicSplitButtonController::SetActorNudgeControllerForTesting(
    std::unique_ptr<GlicActorNudgeController> controller) {
  glic_actor_nudge_controller_ = std::move(controller);
}

}  // namespace glic
