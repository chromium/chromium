// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"

#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "components/multistep_filter/content/filter_initiated_navigation_marker.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
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
    current_url_filter_suggestion_ = std::move(suggestion);
    ShowSuggestionUi(*current_url_filter_suggestion_);
  }
}

base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
FilterUiController::GetSuggestionCallback() {
  return base::BindOnce(&FilterUiController::OnSuggestionGenerated,
                        weak_factory_.GetWeakPtr());
}

void FilterUiController::ClearSuggestion() {
  // TODO(crbug.com/491210510): This method is currently only called when a
  // suggestion is applied or when a new navigation is committed. Update the
  // implementation to also call this when the toast is dismissed to free the
  // cached suggestion memory, ensuring we don't accidentally clear a newly
  // generated suggestion due to async callbacks.
  weak_factory_.InvalidateWeakPtrs();
  if (!current_url_filter_suggestion_) {
    return;
  }
  current_url_filter_suggestion_.reset();
}

void FilterUiController::ApplySuggestion() {
  if (!current_url_filter_suggestion_ ||
      current_url_filter_suggestion_->url().is_empty()) {
    return;
  }
  GURL url = current_url_filter_suggestion_->url();
  ClearSuggestion();
  NavigateTo(url);
}

void FilterUiController::ShowSuggestionUi(
    const UrlFilterSuggestion& suggestion) {
  if (BrowserWindowInterface* browser_window_interface =
          tab().GetBrowserWindowInterface()) {
    if (ToastController* toast_controller =
            browser_window_interface->GetFeatures().toast_controller()) {
      ToastParams params(ToastId::kMultistepFilterSuggestion);
      // TODO(crbug.com/491202866) : Override the suggestion string in the
      // toast.
      toast_controller->MaybeShowToast(std::move(params));
    }
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

}  // namespace multistep_filter
