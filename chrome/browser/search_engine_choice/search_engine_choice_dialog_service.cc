// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service.h"

#include "base/check_deref.h"
#include "base/check_is_test.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/debug/crash_logging.h"
#include "base/not_fatal_until.h"
#include "base/notreached.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engine_choice/search_engine_choice_dialog_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/profiles/profile_customization_bubble_sync_controller.h"
#include "chrome/browser/ui/search_engine_choice/search_engine_choice_tab_helper.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "components/country_codes/country_codes.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/prepopulated_engines.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_utils.h"
#include "components/search_engines/search_engines_pref_names.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/util.h"

namespace {
bool g_dialog_disabled_for_testing = false;

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

// --- SearchEngineChoiceDialogService::BrowserRegistry -----------------------

SearchEngineChoiceDialogService::BrowserRegistry::BrowserRegistry(
    SearchEngineChoiceDialogService& service)
    : search_engine_choice_dialog_service_(service) {
  observation_.Observe(BrowserList::GetInstance());
}

SearchEngineChoiceDialogService::BrowserRegistry::~BrowserRegistry() {
  CloseAllDialogs();
}

bool SearchEngineChoiceDialogService::BrowserRegistry::RegisterBrowser(
    Browser& browser,
    base::OnceClosure close_dialog_callback) {
  CHECK(close_dialog_callback);
  if (IsRegistered(browser)) {
    // TODO(crbug.com/347223092): Investigating whether re-registrations
    // are a cause of multi-prompts.
    NOTREACHED(base::NotFatalUntil::M131);
    return false;
  }

  if (registered_browsers_.empty()) {
    // We only need to record that the choice screen was shown once.
    search_engines::RecordChoiceScreenEvent(
        search_engines::SearchEngineChoiceScreenEvents::
            kChoiceScreenWasDisplayed);
  }

  registered_browsers_.emplace(browser, std::move(close_dialog_callback));
  return true;
}

void SearchEngineChoiceDialogService::BrowserRegistry::OnBrowserRemoved(
    Browser* browser) {
  registered_browsers_.erase(CHECK_DEREF(browser));
}

bool SearchEngineChoiceDialogService::BrowserRegistry::IsRegistered(
    Browser& browser) const {
  return base::Contains(registered_browsers_, browser);
}

bool SearchEngineChoiceDialogService::BrowserRegistry::HasOpenDialog(
    Browser& browser) const {
  auto entry_iterator = registered_browsers_.find(browser);
  if (entry_iterator == registered_browsers_.end()) {
    // The browser is not known, so it never showed a dialog.
    return false;
  }

  // If the OnceCallback is null, then the dialog has already been closed.
  return !entry_iterator->second.is_null();
}

void SearchEngineChoiceDialogService::BrowserRegistry::CloseAllDialogs() {
  for (auto& browsers_with_open_dialog : registered_browsers_) {
    if (browsers_with_open_dialog.second) {
      std::move(browsers_with_open_dialog.second).Run();
    }
  }

  // We're not clearing the list to keep track of the browsers that already
  // showed a dialog previously.
}

// --- SearchEngineChoiceDialogService ----------------------------------------

SearchEngineChoiceDialogService::~SearchEngineChoiceDialogService() = default;

SearchEngineChoiceDialogService::SearchEngineChoiceDialogService(
    Profile& profile,
    search_engines::SearchEngineChoiceService& search_engine_choice_service,
    TemplateURLService& template_url_service)
    : profile_(profile),
      search_engine_choice_service_(search_engine_choice_service),
      template_url_service_(template_url_service) {}

void SearchEngineChoiceDialogService::NotifyChoiceMade(
    int prepopulate_id,
    bool save_guest_mode_selection,
    EntryPoint entry_point) {
  int country_id = search_engine_choice_service_->GetCountryId();
  SCOPED_CRASH_KEY_STRING32(
      "ChoiceService", "choice_country",
      country_codes::CountryIDToCountryString(country_id));
  SCOPED_CRASH_KEY_NUMBER("ChoiceService", "prepopulate_id", prepopulate_id);
  SCOPED_CRASH_KEY_NUMBER("ChoiceService", "entry_point",
                          static_cast<int>(entry_point));

  TemplateURL* selected_engine = nullptr;
  int selected_engine_index = -1;
  for (size_t i = 0; i < choice_screen_data_->search_engines().size(); ++i) {
    if (choice_screen_data_->search_engines()[i]->prepopulate_id() ==
        prepopulate_id) {
      selected_engine_index = i;
      selected_engine = choice_screen_data_->search_engines()[i].get();
      break;
    }
  }

  // Checking for states we don't expect to be possible. We are not crashing
  // the browser immediately due to the criticality of the launch and because
  // we have a fallback, which is just letting the user proceed without
  // attempting to apply the choice. Many failure cases come from an unexpected
  // default being included in the list.
  // The conditions are explained below, and we set up crash keys to
  // investigate failures in case they happen.
  // TODO(https://crbug.com/318824817): Clean this up by M127.
  if (
      // The ID associated with the selection was not found in the cached list
      // of search engines. That could be maybe caused by something like
      // https://crbug.com/328041262.
      selected_engine == nullptr ||
      // A custom search engine would have a `prepopulate_id` of 0, We don't
      // expect to trigger the choice screen if it was the current default, per
      // `SearchEngineChoiceService::GetDynamicChoiceScreenConditions`.
      prepopulate_id == 0 ||
      // Distribution custom search engines are not part of the prepopulated
      // data but still have an ID, assigned starting from 1000. We should also
      // not be prompting when that's the default.
      // TODO(crbug.com/324880292): Revisit how we should handle them.
      prepopulate_id > TemplateURLPrepopulateData::kMaxPrepopulatedEngineID) {
    SCOPED_CRASH_KEY_BOOL("ChoiceService", "selected_engine_found",
                          selected_engine != nullptr);

    const TemplateURL* default_search_provider =
        template_url_service_->GetDefaultSearchProvider();
    SCOPED_CRASH_KEY_NUMBER("ChoiceService", "current_dse_id",
                            default_search_provider
                                ? default_search_provider->prepopulate_id()
                                : -1);
    SCOPED_CRASH_KEY_STRING64(
        "ChoiceService", "current_dse_keyword",
        default_search_provider
            ? base::UTF16ToUTF8(default_search_provider->keyword())
            : "<null>");

    NOTREACHED(base::NotFatalUntil::M127);
  } else {
    if (search_engine_choice_service_
            ->IsProfileEligibleForDseGuestPropagation() &&
        save_guest_mode_selection) {
      search_engine_choice_service_->SetSavedSearchEngineBetweenGuestSessions(
          prepopulate_id);
    }
    template_url_service_->SetUserSelectedDefaultSearchProvider(
        selected_engine, search_engines::ChoiceMadeLocation::kChoiceScreen);
  }

  browser_registry_.CloseAllDialogs();

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

  search_engines::ChoiceScreenDisplayState display_state =
      choice_screen_data_->display_state();
  display_state.selected_engine_index = selected_engine_index;

  search_engines::RecordChoiceScreenEvent(event);
  search_engine_choice_service_->MaybeRecordChoiceScreenDisplayState(
      display_state);
}

bool SearchEngineChoiceDialogService::RegisterDialog(
    Browser& browser,
    base::OnceClosure close_dialog_callback) {
  auto condition = ComputeDialogConditions(browser);
  SCOPED_CRASH_KEY_NUMBER("ChoiceService", "dialog_condition",
                          static_cast<int>(condition));
  if (condition !=
      search_engines::SearchEngineChoiceScreenConditions::kEligible) {
    // We expect the caller to have verified that the dialog can actually be
    // shown before attempting to register it.
    NOTREACHED(base::NotFatalUntil::M131);
    return false;
  }

  return browser_registry_.RegisterBrowser(browser,
                                           std::move(close_dialog_callback));
}

// static
void SearchEngineChoiceDialogService::SetDialogDisabledForTests(
    bool dialog_disabled) {
  CHECK_IS_TEST();
  g_dialog_disabled_for_testing = dialog_disabled;
}

// static
search_engines::ChoiceData
SearchEngineChoiceDialogService::GetChoiceDataFromProfile(Profile& profile) {
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
void SearchEngineChoiceDialogService::UpdateProfileFromChoiceData(
    Profile& profile,
    const search_engines::ChoiceData& choice_data) {
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

  const TemplateURLData& default_search_engine =
      choice_data.default_search_engine;
  if (!default_search_engine.keyword().empty() &&
      !default_search_engine.url().empty()) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(&profile);
    CHECK(template_url_service);
    TemplateURL template_url(default_search_engine);
    template_url_service->SetUserSelectedDefaultSearchProvider(&template_url);
  }
}

TemplateURL::TemplateURLVector
SearchEngineChoiceDialogService::GetSearchEngines() {
  if (!choice_screen_data_) {
    choice_screen_data_ = template_url_service_->GetChoiceScreenData();
  }

  TemplateURLService::TemplateURLVector result;
  for (const auto& turl : choice_screen_data_->search_engines()) {
    result.push_back(turl.get());
  }

  return result;
}

search_engines::SearchEngineChoiceScreenConditions
SearchEngineChoiceDialogService::ComputeDialogConditions(
    Browser& browser) const {
  if (g_dialog_disabled_for_testing) {
    return search_engines::SearchEngineChoiceScreenConditions::
        kFeatureSuppressed;
  }

  if (browser_registry_.IsRegistered(browser)) {
    if (browser_registry_.HasOpenDialog(browser)) {
      return search_engines::SearchEngineChoiceScreenConditions::
          kAlreadyBeingShown;
    }

    return search_engines::SearchEngineChoiceScreenConditions::
        kAlreadyCompleted;
  }

  if (search_engine_choice_service_->GetSavedSearchEngineBetweenGuestSessions()
          .has_value()) {
    return search_engines::SearchEngineChoiceScreenConditions::
        kUsingPersistedGuestSessionChoice;
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
      search_engine_choice_service_->GetDynamicChoiceScreenConditions(
          *template_url_service_);
  if (dynamic_conditions !=
      search_engines::SearchEngineChoiceScreenConditions::kEligible) {
    return dynamic_conditions;
  }

  return search_engines::SearchEngineChoiceScreenConditions::kEligible;
}

bool SearchEngineChoiceDialogService::CanSuppressPrivacySandboxPromo() const {
  return !choice_made_in_profile_picker_;
}

bool SearchEngineChoiceDialogService::IsShowingDialog(Browser& browser) const {
  return browser_registry_.HasOpenDialog(browser);
}

bool SearchEngineChoiceDialogService::HasPendingDialog(Browser& browser) const {
  return browser_registry_.HasOpenDialog(browser) ||
         ComputeDialogConditions(browser) ==
             search_engines::SearchEngineChoiceScreenConditions::kEligible;
}

bool SearchEngineChoiceDialogService::IsUrlSuitableForDialog(GURL url) {
  if (url == chrome::kChromeUINewTabPageURL) {
    return true;  // NTP URL for regular profiles.
  }

  if (NewTabUI::IsNewTab(url)) {
    // This is the NTP URL for Guest and incognito profiles. This service is not
    // instantiated for incognito profiles, so this is only Guest in practice.
    return true;
  }

  if (url == url::kAboutBlankURL) {
    return true;
  }
  if (url.SchemeIs(content::kChromeDevToolsScheme)) {
    return false;
  }
  // Don't show the dialog over remaining urls that start with 'chrome://'.
  return !url.SchemeIs(content::kChromeUIScheme);
}

void SearchEngineChoiceDialogService::NotifyLearnMoreLinkClicked(
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

void SearchEngineChoiceDialogService::NotifyMoreButtonClicked(
    EntryPoint entry_point) {
  search_engines::SearchEngineChoiceScreenEvents event;

  switch (entry_point) {
    case EntryPoint::kDialog:
      event =
          search_engines::SearchEngineChoiceScreenEvents::kMoreButtonClicked;
      break;
    case EntryPoint::kFirstRunExperience:
      event =
          search_engines::SearchEngineChoiceScreenEvents::kFreMoreButtonClicked;
      break;
    case EntryPoint::kProfileCreation:
      event = search_engines::SearchEngineChoiceScreenEvents::
          kProfileCreationMoreButtonClicked;
      break;
  }
  RecordChoiceScreenEvent(event);
}
