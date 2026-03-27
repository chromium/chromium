// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"

#include "base/functional/bind.h"
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

bool FilterUiController::ShouldSuppressSuggestions(const GURL& url) {
  return dismissed_hosts_.contains(GetEtldPlusOne(url));
}

void FilterUiController::ShowSuggestionUi(
    const UrlFilterSuggestion& suggestion) {
  if (BrowserWindowInterface* browser_window_interface =
          tab().GetBrowserWindowInterface()) {
    if (ToastController* toast_controller =
            browser_window_interface->GetFeatures().toast_controller()) {
      ToastParams params(ToastId::kMultistepFilterSuggestion);
      // TODO(crbug.com/491202866): Override the suggestion string in the
      // toast.

      // Associate the dismissal with the URL where the suggestion was shown.
      GURL source_url;
      if (content::WebContents* web_contents = tab().GetContents()) {
        source_url = web_contents->GetLastCommittedURL();
      }

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
