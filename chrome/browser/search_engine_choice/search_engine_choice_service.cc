// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_service.h"

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profiles/profile_customization_bubble_sync_controller.h"
#include "chrome/browser/ui/search_engine_choice/search_engine_choice_tab_helper.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "components/country_codes/country_codes.h"
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

void RecordChoiceScreenNavigationCondition(
    search_engines::SearchEngineChoiceScreenConditions condition) {
  base::UmaHistogramEnumeration(
      search_engines::kSearchEngineChoiceScreenNavigationConditionsHistogram,
      condition);
}

bool IsBrowserTypeSupported(const Browser& browser) {
  switch (browser.type()) {
    case Browser::TYPE_NORMAL:
    case Browser::TYPE_POPUP:
      return true;
    case Browser::TYPE_APP_POPUP:
    case Browser::TYPE_PICTURE_IN_PICTURE:
    case Browser::TYPE_APP:
    case Browser::TYPE_DEVTOOLS:
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case Browser::TYPE_CUSTOM_TAB:
#endif
      return false;
  }
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

void SearchEngineChoiceService::NotifyChoiceMade(int prepopulate_id,
                                                 EntryPoint entry_point) {
  PrefService* pref_service = profile_->GetPrefs();

  // A custom search engine would have a `prepopulate_id` of 0.
  // Having a custom search engine displayed on the choice screen would mean
  // that it is already the default search engine so we don't need to change
  // anything.
  const int kCustomSearchEngineId = 0;
  if (prepopulate_id != kCustomSearchEngineId) {
    std::unique_ptr<TemplateURLData> search_engine =
        TemplateURLPrepopulateData::GetPrepopulatedEngine(pref_service,
                                                          prepopulate_id);

    int country_id =
        search_engines::GetSearchEngineChoiceCountryId(pref_service);
    SCOPED_CRASH_KEY_STRING32(
        "ChoiceService", "choice_country",
        country_codes::CountryIDToCountryString(country_id));
    SCOPED_CRASH_KEY_NUMBER("ChoiceService", "prepopulate_id", prepopulate_id);
    SCOPED_CRASH_KEY_NUMBER("ChoiceService", "entry_point",
                            static_cast<int>(entry_point));
    if (!search_engine) {
      search_engine =
          TemplateURLPrepopulateData::GetPrepopulatedEngineFromFullList(
              pref_service, prepopulate_id);

      SCOPED_CRASH_KEY_BOOL("ChoiceService", "engine_found",
                            search_engine != nullptr);
      base::debug::DumpWithoutCrashing();
    }

    CHECK(search_engine);
    TemplateURL search_engine_template_url = TemplateURL(*search_engine);
    template_url_service_->SetUserSelectedDefaultSearchProvider(
        &search_engine_template_url);
  } else {
    // Make sure that the default search engine is a custom search engine.
    const TemplateURL* default_search_provider =
        template_url_service_->GetDefaultSearchProvider();
    if (!default_search_provider) {
      base::debug::DumpWithoutCrashing();
    } else {
      CHECK_EQ(default_search_provider->prepopulate_id(), 0);
    }
  }

  // Closes the dialogs that are open on other browser windows that
  // have the same profile as the one on which the choice was made.
  for (auto& browsers_with_open_dialog : browsers_with_open_dialogs_) {
    std::move(browsers_with_open_dialog.second).Run();
  }
  browsers_with_open_dialogs_.clear();

  // Log the view entry point in which the choice was made.
  search_engines::SearchEngineChoiceScreenEvents event;
  switch (entry_point) {
    case EntryPoint::kDialog:
      event = search_engines::SearchEngineChoiceScreenEvents::kDefaultWasSet;
      break;
    case EntryPoint::kFirstRunExperience:
      event = search_engines::SearchEngineChoiceScreenEvents::kFreDefaultWasSet;
      choice_made_in_profile_picker_ = true;
      break;
    case EntryPoint::kProfileCreation:
      event = search_engines::SearchEngineChoiceScreenEvents::
          kProfileCreationDefaultWasSet;
      choice_made_in_profile_picker_ = true;
      break;
  }
  search_engines::RecordChoiceScreenEvent(event);

  // `RecordChoiceMade` should always be called after setting the default
  // search engine.
  RecordChoiceMade(pref_service,
                   search_engines::ChoiceMadeLocation::kChoiceScreen,
                   &template_url_service_.get());
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

// static
search_engines::ChoiceData SearchEngineChoiceService::GetChoiceDataFromProfile(
    Profile& profile) {
  if (!search_engines::IsChoiceScreenFlagEnabled(
          search_engines::ChoicePromo::kAny)) {
    return {};
  }

  PrefService* pref_service = profile.GetPrefs();
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(&profile);
  CHECK(template_url_service);
  const TemplateURLData& default_search_engine =
      template_url_service->GetDefaultSearchProvider()->data();

  return {.timestamp = pref_service->GetInt64(
              prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp),
          .chrome_version = pref_service->GetString(
              prefs::kDefaultSearchProviderChoiceScreenCompletionVersion),
          .default_search_engine = default_search_engine};
}

// static
void SearchEngineChoiceService::UpdateProfileFromChoiceData(
    Profile& profile,
    search_engines::ChoiceData& choice_data) {
  if (!search_engines::IsChoiceScreenFlagEnabled(
          search_engines::ChoicePromo::kAny)) {
    return;
  }

  PrefService* pref_service = profile.GetPrefs();
  if (choice_data.timestamp != 0) {
    pref_service->SetInt64(
        prefs::kDefaultSearchProviderChoiceScreenCompletionTimestamp,
        choice_data.timestamp);
  }

  if (!choice_data.chrome_version.empty()) {
    pref_service->SetString(
        prefs::kDefaultSearchProviderChoiceScreenCompletionVersion,
        choice_data.chrome_version);
  }

  TemplateURLData& default_search_engine = choice_data.default_search_engine;
  if (!default_search_engine.keyword().empty() &&
      !default_search_engine.url().empty()) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(&profile);
    CHECK(template_url_service);
    TemplateURL template_url(default_search_engine);
    template_url_service->SetUserSelectedDefaultSearchProvider(&template_url);
  }
}

bool SearchEngineChoiceService::IsShowingDialog(Browser* browser) {
  return base::Contains(browsers_with_open_dialogs_, browser);
}

std::vector<std::unique_ptr<TemplateURL>>
SearchEngineChoiceService::GetSearchEngines() {
  return template_url_service_->GetTemplateURLsForChoiceScreen();
}

search_engines::SearchEngineChoiceScreenConditions
SearchEngineChoiceService::ComputeDialogConditions(Browser& browser) {
  if (!search_engines::IsChoiceScreenFlagEnabled(
          search_engines::ChoicePromo::kDialog)) {
    return search_engines::SearchEngineChoiceScreenConditions::
        kFeatureSuppressed;
  }

  if (web_app::AppBrowserController::IsWebApp(&browser)) {
    // Showing a Chrome-specific search engine dialog on top of a window
    // dedicated to a specific web app is a horrible UX, we suppress it for this
    // window. When the user proceeds to a non-web app window they will get it.
    return search_engines::SearchEngineChoiceScreenConditions::
        kUnsupportedBrowserType;
  }

  // Only show the dialog over normal and popup browsers. This is to avoid
  // showing it in picture-in-picture for example.
  if (!IsBrowserTypeSupported(browser)) {
    return search_engines::SearchEngineChoiceScreenConditions::
        kUnsupportedBrowserType;
  }

  if (!CanWindowHeightFitSearchEngineChoiceDialog(browser)) {
    return search_engines::SearchEngineChoiceScreenConditions::
        kBrowserWindowTooSmall;
  }

  // To avoid conflict, the dialog should not be shown if a sign-in dialog is
  // currently displayed or is about to be displayed.
  bool signin_dialog_displayed_or_pending =
      browser.signin_view_controller()->ShowsModalDialog();
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  signin_dialog_displayed_or_pending =
      signin_dialog_displayed_or_pending ||
      IsProfileCustomizationBubbleSyncControllerRunning(&browser);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  if (signin_dialog_displayed_or_pending) {
    return search_engines::SearchEngineChoiceScreenConditions::
        kSuppressedByOtherDialog;
  }

  // Respect common conditions with other platforms.
  search_engines::SearchEngineChoiceScreenConditions dynamic_conditions =
      search_engines::GetDynamicChoiceScreenConditions(
          CHECK_DEREF(profile_->GetPrefs()), *template_url_service_);
  if (dynamic_conditions !=
      search_engines::SearchEngineChoiceScreenConditions::kEligible) {
    return dynamic_conditions;
  }

  // Lastly, we check if this profile can be the selected one for showing the
  // dialogs. We check it last to make sure we don't mark to eagerly this one
  // as the choice profile if one of the other conditions is not met.
  if (!SearchEngineChoiceServiceFactory::IsSelectedChoiceProfile(
          profile_.get(), /*try_claim=*/true)) {
    return search_engines::SearchEngineChoiceScreenConditions::
        kProfileOutOfScope;
  }

  return search_engines::SearchEngineChoiceScreenConditions::kEligible;
}

bool SearchEngineChoiceService::CanShowDialog(Browser& browser) {
  // Dialog should not be shown if it is currently displayed
  if (g_dialog_disabled_for_testing || IsShowingDialog(&browser)) {
    return false;
  }

  search_engines::SearchEngineChoiceScreenConditions conditions =
      ComputeDialogConditions(browser);
  RecordChoiceScreenNavigationCondition(conditions);

  return conditions ==
         search_engines::SearchEngineChoiceScreenConditions::kEligible;
}

bool SearchEngineChoiceService::CanSuppressPrivacySandboxPromo() const {
  return choice_made_in_profile_picker_;
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

void SearchEngineChoiceService::NotifyLearnMoreLinkClicked(
    EntryPoint entry_point) {
  search_engines::SearchEngineChoiceScreenEvents event;

  switch (entry_point) {
    case EntryPoint::kDialog:
      event = search_engines::SearchEngineChoiceScreenEvents::
          kLearnMoreWasDisplayed;
      break;
    case EntryPoint::kFirstRunExperience:
      event = search_engines::SearchEngineChoiceScreenEvents::
          kFreLearnMoreWasDisplayed;
      break;
    case EntryPoint::kProfileCreation:
      event = search_engines::SearchEngineChoiceScreenEvents::
          kProfileCreationLearnMoreDisplayed;
      break;
  }
  RecordChoiceScreenEvent(event);
}
