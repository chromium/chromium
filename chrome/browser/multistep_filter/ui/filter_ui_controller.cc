// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"

#include <cmath>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/multistep_filter/core/multistep_filter_log_router_factory.h"
#include "chrome/browser/multistep_filter/core/multistep_filter_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/page_action/action_ids.h"
#include "chrome/browser/ui/page_action/page_action_controller.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/favicon/core/favicon_service.h"
#include "components/multistep_filter/content/filter_initiated_navigation_marker.h"
#include "components/multistep_filter/core/logging/log_entry.h"
#include "components/multistep_filter/core/logging/multistep_filter_logger.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
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

// TODO(crbug.com/515670907): Remove 'const' from parameters passed by
// value in .cc file as per code review feedback.
void LogUiAccepted(MultistepFilterLogRouter* const log_router,
                   const int64_t navigation_id,
                   const std::string_view triggering_domain) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id, LogEventType::kUiAccepted,
                       triggering_domain)
      << LogDetail{"navigation_attempted", true};
}

void LogUiShown(MultistepFilterLogRouter* const log_router,
                const UrlFilterSuggestion& suggestion,
                bool ui_shown) {
  MULTISTEP_FILTER_LOG(log_router, suggestion.triggering_navigation_id,
                       LogEventType::kUiShown, suggestion.triggering_domain)
      << LogDetail{"ui_shown", ui_shown};
}

void LogUiDismissed(MultistepFilterLogRouter* const log_router,
                    const int64_t navigation_id,
                    const std::string_view triggering_domain,
                    const std::string_view suppressed_domain) {
  MULTISTEP_FILTER_LOG(log_router, navigation_id, LogEventType::kUiDismissed,
                       triggering_domain)
      << LogDetail{"suppressed_domain", std::string(suppressed_domain)};
}

}  // namespace

DEFINE_USER_DATA(FilterUiController);

FilterUiController::SuggestionUiData::SuggestionUiData(
    ToastId toast_id,
    std::vector<std::u16string> replacement_params)
    : toast_id(toast_id), replacement_params(std::move(replacement_params)) {}

FilterUiController::SuggestionUiData::SuggestionUiData(
    const SuggestionUiData&) = default;

FilterUiController::SuggestionUiData&
FilterUiController::SuggestionUiData::operator=(const SuggestionUiData&) =
    default;

FilterUiController::SuggestionUiData::~SuggestionUiData() = default;

// static
FilterUiController* FilterUiController::From(tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

FilterUiController::FilterUiController(tabs::TabInterface& tab)
    : tabs::ContentsObservingTabFeature(tab),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  if (Profile* profile = tab.GetProfile()) {
    log_router_ = MultistepFilterLogRouterFactory::GetForProfile(profile);
    service_ = MultistepFilterServiceFactory::GetForProfile(profile);
    favicon_service_ = FaviconServiceFactory::GetForProfile(
        profile, ServiceAccessType::EXPLICIT_ACCESS);
  }
  if (tab.GetTabFeatures()) {
    page_action_controller_ = tab.GetTabFeatures()->page_action_controller();
  }
}

FilterUiController::~FilterUiController() = default;

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
      DismissSuggestion();
      break;
    case internal::kSettingsCommand:
      OpenSettings();
      break;
  }
  ClearSuggestion();
}

void FilterUiController::DismissSuggestion() {
  if (current_url_filter_suggestion_) {
    content::WebContents* web_contents = tab().GetContents();
    std::string dismissal_domain =
        web_contents ? GetEtldPlusOne(web_contents->GetLastCommittedURL()) : "";
    LogUiDismissed(
        log_router_, current_url_filter_suggestion_->triggering_navigation_id,
        current_url_filter_suggestion_->triggering_domain, dismissal_domain);
  }
}

void FilterUiController::OpenSettings() {
  // TODO(crbug.com/517999412): Use Delegate pattern to avoid circular
  // dependency and use chrome::ShowSettingsSubPage instead of manual
  // navigation.
  if (content::WebContents* web_contents = tab().GetContents()) {
    GURL settings_url(chrome::kChromeUISettingsURL);
    content::OpenURLParams params(
        settings_url.Resolve(chrome::kExperimentalAISettingsSubPage),
        content::Referrer(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui::PAGE_TRANSITION_GENERATED,
        /*is_renderer_initiated=*/false);
    web_contents->OpenURL(params,
                          base::BindOnce([](content::NavigationHandle&) {}));
  }
}

void FilterUiController::OnSuggestionGenerated(
    std::optional<UrlFilterSuggestion> suggestion) {
  if (!suggestion) {
    return;
  }
  if (!tab().GetContents() || !service_ || !page_action_controller_ ||
      !favicon_service_) {
    LogUiShown(log_router_, *suggestion, false);
    return;
  }

  // Clear any existing suggestion state before showing the new one.
  ClearSuggestion();

  // TODO(crbug.com/514312241): Clean up toast code once cue is ready.
  ShowCue(*suggestion);
  service_->DeleteAnnotationsForTask(suggestion->task_type,
                                     suggestion->triggering_navigation_id,
                                     suggestion->triggering_domain);
  LogUiShown(log_router_, *suggestion, true);
  current_url_filter_suggestion_ = std::move(suggestion);
}

void FilterUiController::ClearSuggestion() {
  dismissal_weak_factory_.InvalidateWeakPtrs();
  if (!current_url_filter_suggestion_) {
    return;
  }
  current_url_filter_suggestion_.reset();
  ClearCue();
}

void FilterUiController::ApplySuggestion() {
  if (!current_url_filter_suggestion_ ||
      current_url_filter_suggestion_->navigation_url.is_empty()) {
    return;
  }

  std::string_view domain = current_url_filter_suggestion_->triggering_domain;
  LogUiAccepted(log_router_,
                current_url_filter_suggestion_->triggering_navigation_id,
                domain);

  GURL url = current_url_filter_suggestion_->navigation_url;
  // Clearing the suggestion prevents the toast close callback from marking
  // this as a dismissal because it invalidates the dismissal weak pointers.
  ClearSuggestion();
  NavigateTo(url);
}

void FilterUiController::OnActionInvoked() {
  if (!current_url_filter_suggestion_) {
    return;
  }
  if (page_action_controller_ &&
      page_action_controller_->GetActiveAnchoredMessage() ==
          kActionMultistepFilter) {
    ApplySuggestion();
  } else {
    ShowCue(*current_url_filter_suggestion_);
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

// TODO(crbug.com/514312241): Clean up toast code once cue is ready.
FilterUiController::SuggestionUiData FilterUiController::GetSuggestionUiData(
    const UrlFilterSuggestion& suggestion,
    base::Time now) const {
  int num_filters = suggestion.attribute_ui_labels.size();
  base::TimeDelta suggestion_age = now - suggestion.extraction_timestamp;
  if (suggestion_age.is_negative()) {
    suggestion_age = base::TimeDelta();
  }

  std::vector<std::u16string> attribute_strings;
  std::ranges::transform(suggestion.attribute_ui_labels,
                         std::back_inserter(attribute_strings),
                         [](const FilterAttributeUiLabel& label) {
                           return label.attribute_value;
                         });
  std::u16string filter_names = base::JoinString(attribute_strings, u", ");

  // Show the user the ETLD+1 of the domain where the suggestion was generated
  // from if the suggestion is less than a day old.
  if (suggestion_age < base::Days(1)) {
    return {/*toast_id=*/ToastId::kMultistepFilterSuggestionRecent,
            /*replacement_params=*/{base::NumberToString16(num_filters),
                                    suggestion.source_domain, filter_names}};
  }

  std::u16string age_description;
  if (suggestion_age < base::Days(30)) {
    age_description = l10n_util::GetPluralStringFUTF16(IDS_TIME_DAYS,
                                                       suggestion_age.InDays());
  } else {
    int num_months =
        static_cast<int>(std::round(suggestion_age / base::Days(30)));
    age_description =
        l10n_util::GetPluralStringFUTF16(IDS_TIME_MONTHS, num_months);
  }

  return {/*toast_id=*/ToastId::kMultistepFilterSuggestion,
          /*replacement_params=*/{base::NumberToString16(num_filters),
                                  age_description, filter_names}};
}

// TODO(crbug.com/514312241): Clean up toast code once cue is ready.
bool FilterUiController::ShowSuggestionUi(ToastParams params) {
  BrowserWindowInterface* browser_window_interface =
      tab().GetBrowserWindowInterface();
  if (!browser_window_interface) {
    return false;
  }

  ToastController* toast_controller =
      browser_window_interface->GetFeatures().toast_controller();
  if (!toast_controller) {
    return false;
  }

  return toast_controller->MaybeShowToast(std::move(params));
}

// TODO(crbug.com/514312241): Clean up toast code once cue is ready.
base::OnceClosure FilterUiController::GetOnDismissedCallback(
    std::string dismissal_domain,
    int64_t navigation_id,
    std::string triggering_domain) {
  return base::BindOnce(&FilterUiController::OnSuggestionDismissed,
                        dismissal_weak_factory_.GetWeakPtr(),
                        std::move(dismissal_domain), navigation_id,
                        std::move(triggering_domain));
}

// TODO(crbug.com/514312241): Clean up toast code once cue is ready.
void FilterUiController::OnSuggestionDismissed(std::string dismissal_domain,
                                               int64_t navigation_id,
                                               std::string triggering_domain) {
  LogUiDismissed(log_router_, navigation_id, triggering_domain,
                 dismissal_domain);
  // This invalidates the weak pointers, including the one that triggered this
  // callback, making it a OnceClosure effectively.
  ClearSuggestion();
}

void FilterUiController::ShowCue(const UrlFilterSuggestion& suggestion) {
  // Fetch favicon for the suggestion domain.
  GURL domain_url(
      base::StrCat({"https://", base::UTF16ToUTF8(suggestion.source_domain)}));
  favicon_service_->GetFaviconImageForPageURL(
      domain_url,
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
  menu_model->AddItem(
      internal::kDismissCommand,
      l10n_util::GetStringUTF16(IDS_MULTISTEP_FILTER_CUE_DISMISS));
  menu_model->AddItem(
      internal::kSettingsCommand,
      l10n_util::GetStringUTF16(IDS_MULTISTEP_FILTER_CUE_SETTINGS));
  page_action_controller_->SetAnchoredMessageAction(
      kActionMultistepFilter,
      page_actions::AnchoredMessageActionIconType::kMenu,
      std::move(menu_model));

  std::vector<page_actions::AnchoredMessageExpandableItem> items;
  items.push_back(
      {.icon = result.image.IsEmpty()
                   ? ui::ImageModel::FromVectorIcon(vector_icons::kGlobeIcon)
                   : ui::ImageModel::FromImage(result.image),
       .text = suggestion.source_domain});

  page_actions::AnchoredMessageExpandableContent content;
  content.items = std::move(items);

  page_action_controller_->SetAnchoredMessageExpandableContent(
      kActionMultistepFilter, std::move(content));

  page_action_controller_->Show(kActionMultistepFilter);

  page_action_controller_->ShowAnchoredMessage(
      kActionMultistepFilter,
      {.priority = page_actions::PageActionPriorityCategory::kContextualCue});
}

}  // namespace multistep_filter
