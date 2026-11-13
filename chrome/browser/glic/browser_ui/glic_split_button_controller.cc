// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/browser_ui/glic_split_button_controller.h"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "base/feature_list.h"
#include "base/notimplemented.h"
#include "build/build_config.h"
#include "chrome/browser/actor/ui/actor_ui_metrics.h"
#include "chrome/browser/actor/ui/states/actor_task_nudge_state.h"
#include "chrome/browser/glic/browser_ui/activity_list_bubble/glic_activity_list_bubble_controller.h"
#include "chrome/browser/glic/browser_ui/glic_actor_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_button_controller.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller.h"
#include "chrome/browser/glic/browser_ui/glic_nudge_controller_impl.h"
#include "chrome/browser/glic/browser_ui/glic_split_button_delegate.h"
#include "chrome/browser/glic/common/glic_tab_util.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/service/glic_activity_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/base/l10n/l10n_util.h"

namespace glic {

namespace {

bool ShouldCreateActivityListBubbleController(Profile* profile) {
  if (!profile || !profile->IsRegularProfile()) {
    return false;
  }
  if (base::FeatureList::IsEnabled(features::kGlicActivityHub)) {
    return true;
  }
  return base::FeatureList::IsEnabled(features::kGlicActor) &&
         base::FeatureList::IsEnabled(features::kGlicActorUi) &&
         features::kGlicActorUiTaskIcon.Get();
}

std::optional<actor::ui::ActorTaskNudgeState> ToActorTaskNudgeStateForMetrics(
    GlicActivityNudgeState::Text nudge_text) {
  switch (nudge_text) {
    case GlicActivityNudgeState::Text::kNeedsAttention:
      return actor::ui::ActorTaskNudgeState{
          .text = actor::ui::ActorTaskNudgeState::Text::kNeedsAttention};
    case GlicActivityNudgeState::Text::kCompleteTasks:
      return actor::ui::ActorTaskNudgeState{
          .text = actor::ui::ActorTaskNudgeState::Text::kCompleteTasks};
    case GlicActivityNudgeState::Text::kResponseReady:
    case GlicActivityNudgeState::Text::kDefault:
      return std::nullopt;
  }
}

}  // namespace

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

  if (ShouldCreateActivityListBubbleController(browser->GetProfile())) {
    activity_list_bubble_controller_ =
        std::make_unique<GlicActivityListBubbleController>(browser, *this);
    bubble_visibility_change_subscription_.push_back(
        activity_list_bubble_controller_->RegisterBubbleShownCallback(
            base::BindRepeating(
                &GlicSplitButtonController::OnBubbleVisibilityChange,
                weak_factory_.GetWeakPtr(), /*is_bubble_open=*/true)));
    bubble_visibility_change_subscription_.push_back(
        activity_list_bubble_controller_->RegisterBubbleDestroyedCallback(
            base::BindRepeating(
                &GlicSplitButtonController::OnBubbleVisibilityChange,
                weak_factory_.GetWeakPtr(), /*is_bubble_open=*/false)));
    if (!base::FeatureList::IsEnabled(features::kGlicActivityHub)) {
      glic_actor_nudge_controller_ =
          std::make_unique<GlicActorNudgeController>(browser, this);
    }
  }
  if (base::FeatureList::IsEnabled(features::kGlicActivityHub)) {
    InitActivityHub();
  }
}

GlicSplitButtonController::~GlicSplitButtonController() = default;

void GlicSplitButtonController::InitActivityHub() {
  if (auto* tab_list = TabListInterface::From(browser_)) {
    tab_list_observation_.Observe(tab_list);
  }
  activity_subscription_ =
      glic_service_->activity_manager().RegisterActivityNudgeStateChange(
          base::BindRepeating(
              &GlicSplitButtonController::OnActivityNudgeStateChanged,
              base::Unretained(this)));
  UpdateSplitButtonState();
}

void GlicSplitButtonController::SetHorizontalTabsDelegate(
    GlicSplitButtonDelegate* delegate) {
  horizontal_tabs_delegate_ = delegate;
  glic_button_controller_->UpdateButton();
  if (base::FeatureList::IsEnabled(features::kGlicActivityHub)) {
    if (delegate) {
      delegate->UpdateGlicSplitButtonUi(current_ui_state_);
    }
    UpdateSplitButtonState();
  }
}

void GlicSplitButtonController::SetVerticalTabsDelegate(
    GlicSplitButtonDelegate* delegate) {
  vertical_tabs_delegate_ = delegate;
  glic_button_controller_->UpdateButton();
  if (base::FeatureList::IsEnabled(features::kGlicActivityHub)) {
    if (delegate) {
      delegate->UpdateGlicSplitButtonUi(current_ui_state_);
    }
    UpdateSplitButtonState();
  }
}

void GlicSplitButtonController::OnGlicButtonClicked() {
  auto* delegate = GetActiveDelegate();
  if (!delegate) {
    // TODO(crbug.com/511309088): This should not be reachable.
    NOTIMPLEMENTED_LOG_ONCE();
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  // Indicate that the glic button was pressed so that we can either close the
  // IPH promo (if present) or note that it has already been used to prevent
  // unnecessarily displaying the promo.
  if (auto* user_education = BrowserUserEducationInterface::From(browser_)) {
    user_education->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHGlicPromoFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  }
#endif

  const bool is_showing_nudge = delegate->GetIsShowingGlicNudge();
  std::optional<std::string> prompt_suggestion =
      glic_nudge_controller_->GetPromptSuggestion();
  glic_nudge_controller_->ClearPromptSuggestion();

  const bool is_panel_showing =
      glic_service_->IsPanelShowingForBrowser(*browser_);
  tabs::TabInterface* active_tab = GetActiveTab(browser_);
  if (!is_panel_showing && is_showing_nudge && prompt_suggestion &&
      !prompt_suggestion->empty() && active_tab &&
      GlicEnabling::IsEnabledForProfile(browser_->GetProfile())) {
    glic::GlicInvokeOptions options(glic::Target(*active_tab),
                                    GetInvocationSource(*delegate));
    options.prompts.push_back(std::move(*prompt_suggestion));
    glic_service_->Invoke(std::move(options));
  } else {
    glic_service_->ToggleUI(browser_,
                            /*prevent_close=*/false,
                            GetInvocationSource(*delegate));
  }

  if (is_showing_nudge) {
    glic_nudge_controller_->OnNudgeActivity(
        glic::GlicNudgeActivity::kNudgeClicked);
  }
}

void GlicSplitButtonController::ShowBubble() {
  if (activity_list_bubble_controller_) {
    activity_list_bubble_controller_->ShowBubble();
  }
}

void GlicSplitButtonController::CloseBubble() {
  if (activity_list_bubble_controller_) {
    activity_list_bubble_controller_->CloseBubble();
  }
}

void GlicSplitButtonController::OnSecondaryButtonClicked() {
  if (activity_list_bubble_controller_ &&
      activity_list_bubble_controller_->IsBubbleShowing()) {
    CloseBubble();
    return;
  }

  // TODO(b/565017590): Record unified Activity Hub secondary-button click
  // metrics across chat and actor states.
  if (base::FeatureList::IsEnabled(features::kGlicActivityHub)) {
    if (auto nudge_state =
            ToActorTaskNudgeStateForMetrics(glic_service_->activity_manager()
                                                .GetCurrentActivityNudgeState()
                                                .text)) {
      actor::ui::LogGlobalTaskIndicatorClick(*nudge_state);
    }
  }
  ShowBubble();
}

void GlicSplitButtonController::UpdateSplitButtonState() {
  if (!base::FeatureList::IsEnabled(features::kGlicActivityHub)) {
    return;
  }

  GlicSplitButtonUiState new_state = ComputeSplitButtonUiState();
  if (current_ui_state_ == new_state) {
    return;
  }
  current_ui_state_ = std::move(new_state);

  CallOnBoth(base::BindRepeating(
      [](const GlicSplitButtonUiState& state,
         GlicSplitButtonDelegate& delegate) {
        delegate.UpdateGlicSplitButtonUi(state);
      },
      current_ui_state_));

  if (current_ui_state_.secondary_action ==
          GlicSplitButtonUiState::SecondaryAction::kText &&
      !current_ui_state_.secondary_label.empty()) {
    if (auto* delegate = GetActiveDelegate();
        delegate && delegate->GetIsShowingGlicActorTaskIconNudge()) {
      if (auto nudge_state = ToActorTaskNudgeStateForMetrics(
              glic_service_->activity_manager()
                  .GetCurrentActivityNudgeState()
                  .text)) {
        actor::ui::RecordGlobalTaskIndicatorNudgeShown(*nudge_state);
      }
    }
  }
}

GlicSplitButtonUiState GlicSplitButtonController::ComputeSplitButtonUiState()
    const {
  GlicSplitButtonUiState state;

  auto& activity_manager = glic_service_->activity_manager();
  GlicActivityNudgeState activity_nudge_state =
      activity_manager.GetCurrentActivityNudgeState();
  const size_t count = activity_nudge_state.count;

  // Secondary action priority ladder: active alert nudges take precedence
  // over the split chevron, reverting to the chevron once viewed.
  switch (activity_nudge_state.text) {
    case GlicActivityNudgeState::Text::kNeedsAttention:
      state.secondary_action = GlicSplitButtonUiState::SecondaryAction::kText;
      state.secondary_label = l10n_util::GetPluralStringFUTF16(
          IDS_ACTOR_TASK_NUDGE_CHECK_TASK_LABEL, std::max<size_t>(1, count));
      break;
    case GlicActivityNudgeState::Text::kCompleteTasks:
      state.secondary_action = GlicSplitButtonUiState::SecondaryAction::kText;
      state.secondary_label = l10n_util::GetPluralStringFUTF16(
          IDS_ACTOR_TASK_NUDGE_TASK_COMPLETE_LABEL, std::max<size_t>(1, count));
      break;
    case GlicActivityNudgeState::Text::kResponseReady:
      state.secondary_action = GlicSplitButtonUiState::SecondaryAction::kText;
      state.secondary_label =
          l10n_util::GetStringUTF16(IDS_GLIC_ACTIVITY_RESPONSE_READY);
      break;
    case GlicActivityNudgeState::Text::kDefault:
      if (HasSecondaryActivities()) {
        state.secondary_action =
            GlicSplitButtonUiState::SecondaryAction::kChevron;
      }
      break;
  }

  return state;
}

bool GlicSplitButtonController::HasSecondaryActivities() const {
  return !glic_service_->activity_manager()
              .GetSortedActivities(GetActiveTab(browser_))
              .empty();
}

void GlicSplitButtonController::OnActiveTabChanged(TabListInterface& tab_list,
                                                   tabs::TabInterface* tab) {
  UpdateSplitButtonState();
  if (current_ui_state_.secondary_action ==
      GlicSplitButtonUiState::SecondaryAction::kNone) {
    CloseBubble();
  } else if (activity_list_bubble_controller_ &&
             activity_list_bubble_controller_->IsBubbleShowing()) {
    ShowBubble();
  }
}

void GlicSplitButtonController::OnTabListDestroyed(TabListInterface& tab_list) {
  tab_list_observation_.Reset();
}

void GlicSplitButtonController::OnActivityNudgeStateChanged(
    bool show_bubble,
    GlicActivityNudgeState state) {
  UpdateSplitButtonState();
  if (show_bubble) {
    ShowBubble();
  } else if (current_ui_state_.secondary_action ==
             GlicSplitButtonUiState::SecondaryAction::kNone) {
    CloseBubble();
  }
}

void GlicSplitButtonController::OnBubbleVisibilityChange(bool is_bubble_open) {
  CallOnBoth(base::BindRepeating(
      [](bool is_bubble_open, GlicSplitButtonDelegate& delegate) {
        delegate.SetGlicActorNudgePressedState(is_bubble_open);
      },
      is_bubble_open));
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

void GlicSplitButtonController::SetActorNudgeControllerForTesting(
    std::unique_ptr<GlicActorNudgeController> controller) {
  glic_actor_nudge_controller_ = std::move(controller);
}

}  // namespace glic
