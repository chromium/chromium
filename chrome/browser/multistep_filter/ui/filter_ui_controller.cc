// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace multistep_filter {

DEFINE_USER_DATA(FilterUiController);

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
  if (suggestion) {
    current_url_filter_suggestion_ = suggestion;
    ShowSuggestionUi(*suggestion);
  }
}

base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
FilterUiController::GetSuggestionCallback() {
  return base::BindOnce(&FilterUiController::OnSuggestionGenerated,
                        weak_factory_.GetWeakPtr());
}

void FilterUiController::ClearSuggestion() {
  weak_factory_.InvalidateWeakPtrs();
  if (!current_url_filter_suggestion_) {
    return;
  }
  current_url_filter_suggestion_.reset();
  HideSuggestionUi();
}

void FilterUiController::ApplySuggestion() {
  if (!current_url_filter_suggestion_ ||
      current_url_filter_suggestion_->url().is_empty()) {
    return;
  }
  NavigateTo(current_url_filter_suggestion_->url());
}

void FilterUiController::ShowSuggestionUi(
    const UrlFilterSuggestion& suggestion) {
  // TODO(crbug.com/484315974): Implement the logic to show the suggestion UI.
  NOTIMPLEMENTED();
}

void FilterUiController::HideSuggestionUi() {
  // TODO(crbug.com/484315974): Implement the logic to hide the suggestion UI.
  NOTIMPLEMENTED();
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
  web_contents->OpenURL(params, /*navigation_handle_callback=*/{});
}

}  // namespace multistep_filter
