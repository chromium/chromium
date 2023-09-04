// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"

namespace {
bool g_dialog_disabled_for_testing = false;
}

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

SearchEngineChoiceService::SearchEngineChoiceService(Profile& profile)
    : profile_(profile) {}

void SearchEngineChoiceService::NotifyChoiceMade(int prepopulate_id) {
  // Sets the timestamp and search engine choice preferences.
  PrefService* pref_service = profile_->GetPrefs();
  pref_service->SetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
      base::Time::Now().ToDeltaSinceWindowsEpoch().InSeconds());

  // A custom search engine would have a `prepopulate_id` of 0.
  // Having a custom search engine displayed on the choice screen would mean
  // that it is already the default search engine so we don't need to change
  // anything.
  const int kCustomSearchEngineId = 0;
  if (prepopulate_id != kCustomSearchEngineId) {
    std::unique_ptr<TemplateURLData> search_engine =
        TemplateURLPrepopulateData::GetPrepopulatedEngine(pref_service,
                                                          prepopulate_id);
    CHECK(search_engine);
    SetDefaultSearchProviderPrefValue(*pref_service, search_engine->sync_guid);
  } else {
    // Make sure that the default search engine is a custom search engine.
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(&profile_.get());
    const TemplateURL* default_search_provider =
        template_url_service->GetDefaultSearchProvider();

    CHECK_EQ(default_search_provider->prepopulate_id(), 0);
  }

  // Closes the dialogs that are open on other browser windows that
  // have the same profile as the one on which the choice was made.
  for (auto& browsers_with_open_dialog : browsers_with_open_dialogs_) {
    std::move(browsers_with_open_dialog.second).Run();
  }
  browsers_with_open_dialogs_.clear();
}

void SearchEngineChoiceService::NotifyDialogOpened(
    Browser* browser,
    base::OnceClosure close_dialog_callback) {
  CHECK(close_dialog_callback);
  CHECK(!browsers_with_open_dialogs_.count(browser));
  browsers_with_open_dialogs_.emplace(browser,
                                      std::move(close_dialog_callback));
}

void SearchEngineChoiceService::NotifyDialogClosed(Browser* browser) {
  CHECK(base::Contains(browsers_with_open_dialogs_, browser));
  browsers_with_open_dialogs_.erase(browser);
}

// static
void SearchEngineChoiceService::SetDialogDisabledForTests(
    bool dialog_disabled) {
  CHECK_IS_TEST();
  g_dialog_disabled_for_testing = dialog_disabled;
}

bool SearchEngineChoiceService::IsShowingDialog(Browser* browser) {
  return base::Contains(browsers_with_open_dialogs_, browser);
}

std::vector<std::unique_ptr<TemplateURLData>>
SearchEngineChoiceService::GetSearchEngines() {
  auto* pref_service = profile_->GetPrefs();
  return TemplateURLPrepopulateData::GetPrepopulatedEnginesForChoiceScreen(
      pref_service);
}

bool SearchEngineChoiceService::CanShowDialog(Browser& browser) {
  PrefService* prefs = browser.profile()->GetPrefs();

  // Dialog should not be shown if it is currently displayed or if the pref was
  // already set.
  return !prefs->GetInt64(
             prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp) &&
         !IsShowingDialog(&browser) && !g_dialog_disabled_for_testing;
}

bool SearchEngineChoiceService::HasPendingDialog(Browser& browser) {
  return IsShowingDialog(&browser) || CanShowDialog(browser);
}

bool SearchEngineChoiceService::IsUrlSuitableForDialog(GURL url) {
  if (url == chrome::kChromeUINewTabPageURL || url == url::kAboutBlankURL) {
    return true;
  }
  // Don't show the dialog over remaining urls that start with 'chrome://'.
  return !url.SchemeIs(content::kChromeUIScheme);
}
