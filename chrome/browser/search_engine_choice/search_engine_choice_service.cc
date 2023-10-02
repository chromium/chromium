// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"

#include "base/check_is_test.h"
#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/startup/first_run_service.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"

namespace {
bool g_dialog_disabled_for_testing = false;

// Checks that the profile is the first profile that sees the search engine
// choice dialog.
bool IsSelectedChoiceProfile(Profile& profile, PrefService* local_state) {
  base::CommandLine* const command_line =
      base::CommandLine::ForCurrentProcess();
  // Force-enable the choice screen for testing the screen itself.
  if (command_line->HasSwitch(switches::kForceSearchEngineChoiceScreen)) {
    return true;
  }

  if (!local_state->HasPrefPath(prefs::kSearchEnginesChoiceProfile)) {
    local_state->SetFilePath(prefs::kSearchEnginesChoiceProfile,
                             profile.GetBaseName());
    return true;
  }

  return profile.GetBaseName() ==
         local_state->GetFilePath(prefs::kSearchEnginesChoiceProfile);
}

void RecordChoiceScreenNavigationCondition(
    search_engines::SearchEngineChoiceScreenConditions condition) {
  base::UmaHistogramEnumeration(
      search_engines::kSearchEngineChoiceScreenNavigationConditionsHistogram,
      condition);
}

}  // namespace

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

SearchEngineChoiceService::SearchEngineChoiceService(
    Profile& profile,
    TemplateURLService& template_url_service)
    : profile_(profile), template_url_service_(template_url_service) {}

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
    const TemplateURL* default_search_provider =
        template_url_service_->GetDefaultSearchProvider();

    CHECK_EQ(default_search_provider->prepopulate_id(), 0);
  }

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  // Log that the choice was made during the FRE.
  if (FirstRunService* first_run_service =
          FirstRunServiceFactory::GetForBrowserContextIfExists(
              &profile_.get())) {
    choice_made_in_fre_ = true;
  }
#endif

  // Closes the dialogs that are open on other browser windows that
  // have the same profile as the one on which the choice was made.
  for (auto& browsers_with_open_dialog : browsers_with_open_dialogs_) {
    std::move(browsers_with_open_dialog.second).Run();
  }
  browsers_with_open_dialogs_.clear();

  search_engines::RecordChoiceScreenEvent(
      search_engines::SearchEngineChoiceScreenEvents::kDefaultWasSet);
}

void SearchEngineChoiceService::NotifyDialogOpened(
    Browser* browser,
    base::OnceClosure close_dialog_callback) {
  CHECK(close_dialog_callback);
  CHECK(!browsers_with_open_dialogs_.count(browser));
  if (browsers_with_open_dialogs_.empty()) {
    // We only need to record that the choice screen was shown once.
    search_engines::RecordChoiceScreenEvent(
        search_engines::SearchEngineChoiceScreenEvents::
            kChoiceScreenWasDisplayed);
  }
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

// static
void SearchEngineChoiceService::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterFilePathPref(prefs::kSearchEnginesChoiceProfile,
                                 base::FilePath());
}

bool SearchEngineChoiceService::IsShowingDialog(Browser* browser) {
  return base::Contains(browsers_with_open_dialogs_, browser);
}

std::vector<std::unique_ptr<TemplateURLData>>
SearchEngineChoiceService::GetSearchEngines() {
  // We call `GetPrepopulatedEngines` instead of
  // `GetSearchProvidersUsingLoadedEngines` because the latter will return the
  // list of search engines that might have been modified by the user (by
  // changing the engine's keyword in settings for example).
  PrefService* pref_service = profile_->GetPrefs();
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(&profile_.get());
  return TemplateURLPrepopulateData::GetPrepopulatedEngines(
      pref_service,
      /*default_search_provider_index=*/nullptr,
      /*include_current_default=*/true, template_url_service);
}

bool SearchEngineChoiceService::CanShowDialog(Browser& browser) {
  if (!IsSelectedChoiceProfile(profile_.get(),
                               g_browser_process->local_state())) {
    RecordChoiceScreenNavigationCondition(
        search_engines::SearchEngineChoiceScreenConditions::kProfileOutOfScope);
    return false;
  }

  if (web_app::AppBrowserController::IsWebApp(&browser)) {
    // Showing a Chrome-specific search engine dialog on top of a window
    // dedicated to a specific web app is a horrible UX, we suppress it for this
    // window. When the user proceeds to a non-web app window they will get it.
    return false;
  }

  // To avoid conflict, the dialog should not be shown if a sign-in dialog is
  // being currently displayed.
  if (browser.signin_view_controller()->ShowsModalDialog()) {
    return false;
  }

  // Don't show the dialog if the default search engine is set by an extension.
  if (template_url_service_->IsExtensionControlledDefaultSearch()) {
    RecordChoiceScreenNavigationCondition(
        search_engines::SearchEngineChoiceScreenConditions::
            kExtensionContolled);
    return false;
  }

  if (HasUserMadeChoice()) {
    RecordChoiceScreenNavigationCondition(
        search_engines::SearchEngineChoiceScreenConditions::kAlreadyCompleted);
    return false;
  }

  // Dialog should not be shown if it is currently displayed
  if (g_dialog_disabled_for_testing || IsShowingDialog(&browser)) {
    return false;
  }

  RecordChoiceScreenNavigationCondition(
      search_engines::SearchEngineChoiceScreenConditions::kEligible);
  return true;
}

bool SearchEngineChoiceService::HasUserMadeChoice() const {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceSearchEngineChoiceScreen)) {
    return false;
  }
  PrefService* pref_service = profile_->GetPrefs();
  return pref_service->GetInt64(
      prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp);
}

bool SearchEngineChoiceService::WasChoiceMadeInFRE() const {
  return choice_made_in_fre_;
}

bool SearchEngineChoiceService::HasPendingDialog(Browser& browser) {
  return IsShowingDialog(&browser) || CanShowDialog(browser);
}

bool SearchEngineChoiceService::IsUrlSuitableForDialog(GURL url) {
  if (url == chrome::kChromeUINewTabPageURL || url == url::kAboutBlankURL) {
    return true;
  }
  if (url.SchemeIs(content::kChromeDevToolsScheme)) {
    return false;
  }
  // Don't show the dialog over remaining urls that start with 'chrome://'.
  return !url.SchemeIs(content::kChromeUIScheme);
}
