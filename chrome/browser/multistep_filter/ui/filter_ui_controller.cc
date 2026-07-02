// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/contextual_cueing/prefs.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/multistep_filter/core/multistep_filter_log_router_factory.h"
#include "chrome/browser/multistep_filter/core/multistep_filter_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/core/favicon_service.h"
#include "components/multistep_filter/content/filter_initiated_navigation_marker.h"
#include "components/multistep_filter/core/data_models/suggestion_user_decision.h"
#include "components/multistep_filter/core/logging/filter_acceptance_metrics_logger.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_logger.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/optimization_guide/core/feature_registry/feature_registration.h"
#include "components/optimization_guide/core/model_execution/feature_keys.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/gurl.h"

namespace multistep_filter {

namespace {

using SuggestionViewState = FilterUiController::SuggestionViewState;

constexpr std::string_view ViewStateToString(SuggestionViewState state) {
  switch (state) {
    case SuggestionViewState::kInactive:
      return "inactive";
    case SuggestionViewState::kShowingInitialCue:
      return "showing_initial_cue";
    case SuggestionViewState::kCollapsedInOmnibox:
      return "collapsed_in_omnibox";
    case SuggestionViewState::kReopenedFromOmnibox:
      return "reopened_from_omnibox";
    case SuggestionViewState::kCollapsedInOmniboxAfterReopen:
      return "collapsed_in_omnibox_after_reopen";
  }
}

constexpr std::string_view DecisionToString(SuggestionUserDecision decision) {
  switch (decision) {
    case SuggestionUserDecision::kAccepted:
      return "accepted";
    case SuggestionUserDecision::kIgnored:
      return "ignored";
    case SuggestionUserDecision::kDismissed:
      return "dismissed";
    case SuggestionUserDecision::kSettingsOpened:
      return "settings_opened";
  }
}

constexpr LogEventType DecisionToLogEventType(SuggestionUserDecision decision) {
  switch (decision) {
    case SuggestionUserDecision::kAccepted:
      return LogEventType::kSuggestionAccepted;
    case SuggestionUserDecision::kDismissed:
      return LogEventType::kSuggestionDismissed;
    case SuggestionUserDecision::kIgnored:
    case SuggestionUserDecision::kSettingsOpened:
      return LogEventType::kSuggestionIgnored;
  }
}

void LogSuggestionUiDecision(MultistepFilterLogRouter* log_router,
                             const FilterUiController::SuggestionState& state,
                             SuggestionUserDecision decision) {
  LogEventType event_type = DecisionToLogEventType(decision);

  if (decision == SuggestionUserDecision::kAccepted) {
    MULTISTEP_FILTER_LOG(log_router, state.suggestion.triggering_navigation_id,
                         event_type, state.suggestion.triggering_host)
        << LogDetail{"navigation_attempted", true}
        << LogDetail{"view_state", ViewStateToString(state.view_state)}
        << LogDetail{"decision", DecisionToString(decision)};
  } else {
    MULTISTEP_FILTER_LOG(log_router, state.suggestion.triggering_navigation_id,
                         event_type, state.suggestion.triggering_host)
        << LogDetail{"view_state", ViewStateToString(state.view_state)}
        << LogDetail{"decision", DecisionToString(decision)};
  }
}

void LogSuggestionUiShown(MultistepFilterLogRouter* log_router,
                          const UrlFilterSuggestion& suggestion,
                          bool ui_shown,
                          std::string reason) {
  if (reason.empty()) {
    MULTISTEP_FILTER_LOG(log_router, suggestion.triggering_navigation_id,
                         LogEventType::kSuggestionShown,
                         suggestion.triggering_host)
        << LogDetail{"ui_shown", ui_shown};
  } else {
    MULTISTEP_FILTER_LOG(log_router, suggestion.triggering_navigation_id,
                         LogEventType::kSuggestionShown,
                         suggestion.triggering_host)
        << LogDetail{"ui_shown", ui_shown}
        << LogDetail{"reason", std::move(reason)};
  }
}

void RecordMetricsDecision(
    std::optional<FilterUiController::SuggestionState>& state,
    SuggestionUserDecision decision) {
  if (!state || !state->metrics_logger) {
    return;
  }
  switch (state->view_state) {
    case SuggestionViewState::kShowingInitialCue:
      state->metrics_logger->RecordInitialCueAndOverallDecision(decision);
      break;
    case SuggestionViewState::kReopenedFromOmnibox:
      state->metrics_logger->RecordReopenedCueAndOverallDecision(decision);
      break;
    case SuggestionViewState::kCollapsedInOmnibox:
    case SuggestionViewState::kCollapsedInOmniboxAfterReopen:
      break;
    case SuggestionViewState::kInactive:
      NOTREACHED();
  }
  state->metrics_logger.reset();
}

}  // namespace

DEFINE_USER_DATA(FilterUiController);

// static
FilterUiController* FilterUiController::From(tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

FilterUiController::FilterUiController(tabs::TabInterface& tab)
    : tabs::ContentsObservingTabFeature(tab),
      page_actions::PageActionObserver(kActionMultistepFilter),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  if (Profile* profile = tab.GetProfile()) {
    log_router_ = MultistepFilterLogRouterFactory::GetForProfile(profile);
    service_ = MultistepFilterServiceFactory::GetForProfile(profile);
    favicon_service_ = FaviconServiceFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
    pref_service_ = profile->GetPrefs();
  }
  if (tab.GetTabFeatures()) {
    page_action_controller_ = tab.GetTabFeatures()->page_action_controller();
    if (page_action_controller_) {
      RegisterAsPageActionObserver(*page_action_controller_);
    }
  }
}

FilterUiController::~FilterUiController() {
  if (!suggestion_state_ ||
      suggestion_state_->view_state == SuggestionViewState::kInactive) {
    return;
  }
  if (service_) {
    service_->RecordUserInteractionWithSuggestion(
        SuggestionUserDecision::kIgnored);
  }
  LogSuggestionUiDecision(log_router_, *suggestion_state_,
                          SuggestionUserDecision::kIgnored);
  RecordMetricsDecision(suggestion_state_, SuggestionUserDecision::kIgnored);
}

void FilterUiController::OnSuggestionGenerated(
    std::optional<UrlFilterSuggestion> suggestion) {
  if (!suggestion) {
    return;
  }
  if (!tab().GetContents() || !service_ || !page_action_controller_ ||
      !favicon_service_ || !pref_service_) {
    LogSuggestionUiShown(log_router_, *suggestion, false,
                         "missing_dependencies");
    return;
  }
  if (!ShouldShowCue()) {
    LogSuggestionUiShown(log_router_, *suggestion, false,
                         "smart_suggestions_disabled");
    return;
  }

  // Clear any existing suggestion state before showing the new one.
  ClearSuggestion(SuggestionUserDecision::kIgnored);
  suggestion_state_ =
      SuggestionState{.suggestion = std::move(*suggestion),
                      .view_state = SuggestionViewState::kInactive};
  ShowCue(suggestion_state_->suggestion);
}

void FilterUiController::ClearSuggestion(SuggestionUserDecision decision) {
  if (!suggestion_state_) {
    return;
  }
  if (suggestion_state_->view_state != SuggestionViewState::kInactive) {
    if (service_) {
      service_->RecordUserInteractionWithSuggestion(decision);
    }
    LogSuggestionUiDecision(log_router_, *suggestion_state_, decision);
    RecordMetricsDecision(suggestion_state_, decision);
  }
  dismissal_weak_factory_.InvalidateWeakPtrs();
  suggestion_state_.reset();
  ClearCue();
}

void FilterUiController::ApplySuggestion() {
  if (!suggestion_state_ ||
      suggestion_state_->suggestion.navigation_url.is_empty()) {
    return;
  }

  GURL url = suggestion_state_->suggestion.navigation_url;
  ClearSuggestion(SuggestionUserDecision::kAccepted);
  NavigateTo(url);
}

void FilterUiController::OnActionInvoked() {
  if (!suggestion_state_ || !page_action_controller_) {
    return;
  }
  switch (suggestion_state_->view_state) {
    case SuggestionViewState::kShowingInitialCue:
    case SuggestionViewState::kReopenedFromOmnibox:
      ApplySuggestion();
      break;
    case SuggestionViewState::kInactive:
      NOTREACHED();
    case SuggestionViewState::kCollapsedInOmnibox:
    case SuggestionViewState::kCollapsedInOmniboxAfterReopen:
      ShowCue(suggestion_state_->suggestion);
      break;
  }
}

void FilterUiController::NavigateTo(const GURL& url) {
  content::WebContents* web_contents = tab().GetContents();
  if (!web_contents) {
    return;
  }
  content::OpenURLParams params(url, content::Referrer(),
                                WindowOpenDisposition::CURRENT_TAB,
                                ui::PAGE_TRANSITION_GENERATED,
                                /*is_renderer_initiated=*/false);
  web_contents->OpenURL(
      params, base::BindOnce([](content::NavigationHandle& handle) {
        FilterInitiatedNavigationMarker::CreateForNavigationHandle(handle);
      }));
}

// Items in the contextual cue menu are action buttons rather than toggles,
// so they are never checked.
bool FilterUiController::IsCommandIdChecked(int command_id) const {
  return false;
}

// All commands in the contextual cue menu are always enabled when visible.
bool FilterUiController::IsCommandIdEnabled(int command_id) const {
  return true;
}

void FilterUiController::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case internal::kDismissCommand:
      ClearSuggestion(SuggestionUserDecision::kDismissed);
      break;
    case internal::kSettingsCommand:
      ClearSuggestion(SuggestionUserDecision::kSettingsOpened);
      OpenSettings();
      break;
  }
}

void FilterUiController::OpenSettings() {
  // TODO(crbug.com/517999412): Use Delegate pattern to avoid circular
  // dependency and use chrome::ShowSettingsSubPage instead of manual
  // navigation.
  if (content::WebContents* web_contents = tab().GetContents()) {
    GURL settings_url(chrome::kChromeUISettingsURL);
    content::OpenURLParams params(
        settings_url.Resolve(chrome::kSuggestionsSubPage), content::Referrer(),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui::PAGE_TRANSITION_GENERATED,
        /*is_renderer_initiated=*/false);
    web_contents->OpenURL(params,
                          base::BindOnce([](content::NavigationHandle&) {}));
  }
}

bool FilterUiController::ShouldShowCue() const {
  // TODO(b/522733094): Clean this up once proper eligibility integration is
  // complete.
  int opt_in_state = pref_service_->GetInteger(
      optimization_guide::prefs::GetSettingEnabledPrefName(
          optimization_guide::UserVisibleFeatureKey::kContextualCueing));
  if (opt_in_state ==
      std::to_underlying(
          optimization_guide::prefs::FeatureOptInState::kDisabled)) {
    return false;
  }

  // Check enterprise policy.
  if (pref_service_->GetInteger(
          optimization_guide::prefs::kChromeSuggestionsSettings) ==
      std::to_underlying(
          contextual_cueing::ChromeSuggestionsSettingsValue::kDisabled)) {
    return false;
  }

  return true;
}

void FilterUiController::ShowCue(const UrlFilterSuggestion& suggestion) {
  // Fetch favicon for the suggestion source host.
  GURL host_url(
      base::StrCat({"https://", base::UTF16ToUTF8(suggestion.source_host)}));
  favicon_service_->GetFaviconImageForPageURL(
      host_url,
      base::BindOnce(&FilterUiController::OnFaviconAvailable,
                     dismissal_weak_factory_.GetWeakPtr(), suggestion),
      &favicon_task_tracker_);
}

void FilterUiController::ClearCue() {
  if (!page_action_controller_) {
    return;
  }
  page_action_controller_->HideAnchoredMessage(kActionMultistepFilter);
  page_action_controller_->Hide(kActionMultistepFilter);
  page_action_controller_->ClearOverrideText(kActionMultistepFilter);
}

void FilterUiController::OnPageActionAnchoredMessageShown(
    const page_actions::PageActionState& page_action) {
  DCHECK_EQ(page_action.action_id, kActionMultistepFilter);
  if (!suggestion_state_) {
    return;
  }
  switch (suggestion_state_->view_state) {
    case SuggestionViewState::kInactive:
      if (page_action_controller_) {
        page_action_controller_->OverrideText(
            kActionMultistepFilter,
            l10n_util::GetStringUTF16(IDS_MULTISTEP_FILTER_CUE_ACTION_TEXT));
      }
      suggestion_state_->view_state = SuggestionViewState::kShowingInitialCue;
      suggestion_state_->metrics_logger =
          std::make_unique<FilterAcceptanceMetricsLogger>(
              suggestion_state_->suggestion.task_type);
      suggestion_state_->metrics_logger->RecordInitialCueShown();
      LogSuggestionUiShown(log_router_, suggestion_state_->suggestion,
                           /*ui_shown=*/true, /*reason=*/"");
      if (service_) {
        service_->RecordSuggestionImpression();
        // Delete similar suggestions from the service as this one is being
        // shown.
        service_->DeleteAnnotationsForTask(
            suggestion_state_->suggestion.task_type,
            suggestion_state_->suggestion.triggering_navigation_id,
            suggestion_state_->suggestion.triggering_host);
      }
      break;
    case SuggestionViewState::kCollapsedInOmnibox:
    case SuggestionViewState::kCollapsedInOmniboxAfterReopen:
      if (page_action_controller_) {
        page_action_controller_->OverrideText(
            kActionMultistepFilter,
            l10n_util::GetStringUTF16(IDS_MULTISTEP_FILTER_CUE_ACTION_TEXT));
      }
      suggestion_state_->view_state = SuggestionViewState::kReopenedFromOmnibox;
      if (suggestion_state_->metrics_logger) {
        suggestion_state_->metrics_logger->RecordReopenedCueShown();
      }
      break;
    case SuggestionViewState::kShowingInitialCue:
    case SuggestionViewState::kReopenedFromOmnibox:
      NOTREACHED();
  }
}

void FilterUiController::OnPageActionAnchoredMessageHidden(
    const page_actions::PageActionState& page_action) {
  DCHECK_EQ(page_action.action_id, kActionMultistepFilter);
  if (!suggestion_state_) {
    return;
  }

  switch (suggestion_state_->view_state) {
    case SuggestionViewState::kShowingInitialCue:
      LogSuggestionUiDecision(log_router_, *suggestion_state_,
                              SuggestionUserDecision::kIgnored);
      suggestion_state_->view_state = SuggestionViewState::kCollapsedInOmnibox;
      if (page_action_controller_) {
        page_action_controller_->OverrideText(
            kActionMultistepFilter,
            suggestion_state_->suggestion.short_suggestion_message);
      }
      break;
    case SuggestionViewState::kReopenedFromOmnibox:
      LogSuggestionUiDecision(log_router_, *suggestion_state_,
                              SuggestionUserDecision::kIgnored);
      suggestion_state_->view_state =
          SuggestionViewState::kCollapsedInOmniboxAfterReopen;
      if (page_action_controller_) {
        page_action_controller_->OverrideText(
            kActionMultistepFilter,
            suggestion_state_->suggestion.short_suggestion_message);
      }
      break;
    case SuggestionViewState::kInactive:
    case SuggestionViewState::kCollapsedInOmnibox:
    case SuggestionViewState::kCollapsedInOmniboxAfterReopen:
      NOTREACHED();
  }
}

void FilterUiController::OnFaviconAvailable(
    UrlFilterSuggestion suggestion,
    const favicon_base::FaviconImageResult& result) {
  const std::u16string& message = suggestion.suggestion_message;

  page_action_controller_->OverrideText(
      kActionMultistepFilter,
      l10n_util::GetStringUTF16(IDS_MULTISTEP_FILTER_CUE_ACTION_TEXT));

  page_action_controller_->SetAnchoredMessageText(kActionMultistepFilter,
                                                  message);

  auto menu_model = std::make_unique<ui::SimpleMenuModel>(this);
  menu_model->AddItemWithStringIdAndIcon(
      internal::kDismissCommand, IDS_MULTISTEP_FILTER_CUE_DISMISS,
      ui::ImageModel::FromVectorIcon(vector_icons::kCloseIcon));
  menu_model->AddItemWithStringIdAndIcon(
      internal::kSettingsCommand, IDS_MULTISTEP_FILTER_CUE_SETTINGS,
      ui::ImageModel::FromVectorIcon(vector_icons::kSettingsIcon));
  page_action_controller_->SetAnchoredMessageAction(
      kActionMultistepFilter,
      page_actions::AnchoredMessageActionIconType::kMenu,
      std::move(menu_model));

  std::vector<page_actions::AnchoredMessageExpandableItem> items;
  items.push_back(
      {.icon = result.image.IsEmpty()
                   ? ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon)
                   : ui::ImageModel::FromImage(result.image),
       .text = suggestion.source_host});

  page_actions::AnchoredMessageExpandableContent content;
  content.heading = l10n_util::GetStringUTF16(
      IDS_MULTISTEP_FILTER_CUE_EXPANDABLE_CONTENT_HEADING);
  content.items = std::move(items);

  page_action_controller_->SetAnchoredMessageExpandableContent(
      kActionMultistepFilter, std::move(content));

  page_action_controller_->Show(kActionMultistepFilter);

  page_action_controller_->ShowAnchoredMessage(
      kActionMultistepFilter,
      {.priority = page_actions::PageActionPriorityCategory::kContextualCue});
}

}  // namespace multistep_filter
