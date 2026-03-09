// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/multistep_filter/chrome_filter_navigation_observer.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/multistep_filter/core/multistep_filter_service_factory.h"
#include "chrome/browser/multistep_filter/ui/filter_ui_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "components/multistep_filter/content/filter_navigation_observer.h"
#include "components/multistep_filter/core/multistep_filter_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"

namespace multistep_filter {

DEFINE_USER_DATA(ChromeFilterNavigationObserver);

// static
ChromeFilterNavigationObserver* ChromeFilterNavigationObserver::From(
    tabs::TabInterface* tab) {
  return Get(tab->GetUnownedUserDataHost());
}

namespace {

class UiDelegateImpl : public FilterNavigationObserver::UiDelegate {
 public:
  explicit UiDelegateImpl(tabs::TabInterface& tab) : tab_(tab) {}

  void ClearSuggestion() override {
    if (auto* controller = GetController()) {
      controller->ClearSuggestion();
    }
  }

  base::OnceCallback<void(std::optional<UrlFilterSuggestion>)>
  GetSuggestionCallback() override {
    if (auto* controller = GetController()) {
      return controller->GetSuggestionCallback();
    }
    return base::DoNothing();
  }

 private:
  FilterUiController* GetController() {
    return FilterUiController::From(&tab_.get());
  }

  const raw_ref<tabs::TabInterface> tab_;
};

}  // namespace

ChromeFilterNavigationObserver::ChromeFilterNavigationObserver(
    tabs::TabInterface& tab)
    : tabs::ContentsObservingTabFeature(tab),
      scoped_unowned_user_data_(tab.GetUnownedUserDataHost(), *this) {
  UpdateObserver(tab.GetContents());
}

ChromeFilterNavigationObserver::~ChromeFilterNavigationObserver() = default;

void ChromeFilterNavigationObserver::OnDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  UpdateObserver(new_contents);
}

void ChromeFilterNavigationObserver::UpdateObserver(
    content::WebContents* web_contents) {
  observer_.reset();
  if (!web_contents) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  observer_ = std::make_unique<FilterNavigationObserver>(
      web_contents, MultistepFilterServiceFactory::GetForProfile(profile),
      std::make_unique<UiDelegateImpl>(tab()));
}

}  // namespace multistep_filter
