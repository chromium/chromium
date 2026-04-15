// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"

#include <cmath>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "components/multistep_filter/content/filter_initiated_navigation_marker.h"
#include "components/multistep_filter/core/multistep_filter_util.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/strings/grit/ui_strings.h"
#include "url/gurl.h"

namespace multistep_filter {

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
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {}

FilterUiController::~FilterUiController() = default;

void FilterUiController::OnSuggestionGenerated(
    std::optional<UrlFilterSuggestion> suggestion) {
  if (!suggestion) {
    return;
  }

  content::WebContents* web_contents = tab().GetContents();
  if (!web_contents) {
    return;
  }

  const GURL& current_url = web_contents->GetLastCommittedURL();

  if (ShouldSuppressSuggestions(current_url) ||
      IsUrlSubsumedBy(suggestion->navigation_url, current_url)) {
    return;
  }

  // TODO (crbug.com/498897135): Confirm with stakeholders if this check should
  // be removed.
  // Don't show suggestions if there are less than two attributes.
  if (suggestion->attribute_ui_labels.size() <= 1) {
    return;
  }

  // Clear any existing suggestion state before showing the new one.
  ClearSuggestion();
  current_url_filter_suggestion_ = std::move(suggestion);
  ShowSuggestionUi(*current_url_filter_suggestion_);
}

void FilterUiController::ClearSuggestion() {
  dismissal_weak_factory_.InvalidateWeakPtrs();
  if (!current_url_filter_suggestion_) {
    return;
  }
  current_url_filter_suggestion_.reset();
}

void FilterUiController::ApplySuggestion() {
  if (!current_url_filter_suggestion_ ||
      current_url_filter_suggestion_->navigation_url.is_empty()) {
    return;
  }
  GURL url = current_url_filter_suggestion_->navigation_url;
  // Clearing the suggestion prevents the toast close callback from marking
  // this as a dismissal because it invalidates the dismissal weak pointers.
  ClearSuggestion();
  NavigateTo(url);
}

bool FilterUiController::ShouldSuppressSuggestions(const GURL& url) const {
  return dismissed_hosts_.contains(GetEtldPlusOne(url));
}

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

void FilterUiController::ShowSuggestionUi(
    const UrlFilterSuggestion& suggestion) {
  if (BrowserWindowInterface* browser_window_interface =
          tab().GetBrowserWindowInterface()) {
    if (ToastController* toast_controller =
            browser_window_interface->GetFeatures().toast_controller()) {
      // Associate the dismissal with the URL where the suggestion was shown.
      GURL source_url;
      if (content::WebContents* web_contents = tab().GetContents()) {
        source_url = web_contents->GetLastCommittedURL();
      }
      SuggestionUiData data =
          GetSuggestionUiData(suggestion, base::Time::Now());
      ToastParams params(data.toast_id);
      params.body_string_replacement_params =
          std::move(data.replacement_params);
      params.toast_close_callback = base::ScopedClosureRunner(
          base::BindOnce(&FilterUiController::OnSuggestionDismissed,
                         dismissal_weak_factory_.GetWeakPtr(), source_url));
      toast_controller->MaybeShowToast(std::move(params));
    }
  }
}

base::OnceClosure FilterUiController::GetOnDismissedCallback(const GURL& url) {
  return base::BindOnce(&FilterUiController::OnSuggestionDismissed,
                        dismissal_weak_factory_.GetWeakPtr(), url);
}

void FilterUiController::OnSuggestionDismissed(const GURL& url) {
  std::string domain = GetEtldPlusOne(url);
  if (!domain.empty()) {
    dismissed_hosts_.insert(std::move(domain));
  }
  // This invalidates the weak pointers, including the one that triggered this
  // callback, making it a OnceClosure effectively.
  ClearSuggestion();
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

}  // namespace multistep_filter
