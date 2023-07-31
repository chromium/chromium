// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"

#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "components/signin/public/base/signin_switches.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/profiles/profiles_state.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#endif

SearchEngineChoiceService::BrowserObserver::BrowserObserver(
    SearchEngineChoiceService& service)
    : search_engine_choice_service_(service) {
  observation_.Observe(BrowserList::GetInstance());
}

SearchEngineChoiceService::BrowserObserver::~BrowserObserver() {
  observation_.Reset();
}

void SearchEngineChoiceService::BrowserObserver::OnBrowserRemoved(
    Browser* browser) {
  if (search_engine_choice_service_->IsShowingDialog(browser)) {
    search_engine_choice_service_->NotifyDialogClosed(browser);
  }
}

SearchEngineChoiceService::~SearchEngineChoiceService() = default;

SearchEngineChoiceService::SearchEngineChoiceService() = default;

void SearchEngineChoiceService::NotifyDialogOpened(Browser* browser) {
  CHECK(!browsers_with_open_dialogs_.count(browser));
  browsers_with_open_dialogs_.insert(browser);
}

void SearchEngineChoiceService::NotifyDialogClosed(Browser* browser) {
  CHECK(base::Contains(browsers_with_open_dialogs_, browser));
  browsers_with_open_dialogs_.erase(browser);
}

bool SearchEngineChoiceService::IsShowingDialog(Browser* browser) {
  return base::Contains(browsers_with_open_dialogs_, browser);
}

bool SearchEngineChoiceService::ShouldDisplayDialog(Browser& browser) {
  if (!base::FeatureList::IsEnabled(switches::kSearchEngineChoice)) {
    return false;
  }
  auto* search_engine_choice_service =
      SearchEngineChoiceServiceFactory::GetForProfile(browser.profile());
  return search_engine_choice_service &&
         !search_engine_choice_service->IsShowingDialog(&browser);
}
