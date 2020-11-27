// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/metrics/histogram_macros.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/consent_level.h"
#include "extensions/browser/api/feedback_private/feedback_private_api.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/bind.h"
#include "chrome/browser/chromeos/crosapi/browser_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#endif

namespace feedback_private = extensions::api::feedback_private;

namespace chrome {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Returns whether the user has an internal Google account (e.g. @google.com).
bool IsGoogleInternalAccount(Profile* profile) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager)  // Non-GAIA account, e.g. guest mode.
    return false;
  // Browser sync consent is not required to use feedback.
  CoreAccountInfo account_info = identity_manager->GetPrimaryAccountInfo(
      signin::ConsentLevel::kNotRequired);
  return gaia::IsGoogleInternalAccountEmail(account_info.email);
}

// Returns if the feedback page is considered to be triggered from user
// interaction.
bool IsFromUserInteraction(FeedbackSource source) {
  switch (source) {
    case kFeedbackSourceArcApp:
    case kFeedbackSourceAsh:
    case kFeedbackSourceAssistant:
    case kFeedbackSourceBrowserCommand:
    case kFeedbackSourceDesktopTabGroups:
    case kFeedbackSourceMdSettingsAboutPage:
    case kFeedbackSourceOldSettingsAboutPage:
    case kFeedbackSourceTabSearch:
      return true;
    default:
      return false;
  }
}

void OnLacrosActiveTabUrlFeteched(
    Profile* profile,
    chrome::FeedbackSource source,
    const std::string& description_template,
    const std::string& description_placeholder_text,
    const std::string& category_tag,
    const std::string& extra_diagnostics,
    const base::Optional<GURL>& active_tab_url) {
  GURL page_url;
  if (active_tab_url)
    page_url = *active_tab_url;
  chrome::ShowFeedbackPage(page_url, profile, source, description_template,
                           description_placeholder_text, category_tag,
                           extra_diagnostics);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// TODO(http://crbug.com/1132106): Include the following code only in
// non-lacros builds after M87 beta when Feedback crosapi is available in all
// ash versions.
// Calls feedback private api to show Feedback ui.
void RequestFeedbackFlow(const GURL& page_url,
                         Profile* profile,
                         FeedbackSource source,
                         const std::string& description_template,
                         const std::string& description_placeholder_text,
                         const std::string& category_tag,
                         const std::string& extra_diagnostics) {
  extensions::FeedbackPrivateAPI* api =
      extensions::FeedbackPrivateAPI::GetFactoryInstance()->Get(profile);

  feedback_private::FeedbackFlow flow =
      source == kFeedbackSourceSadTabPage
          ? feedback_private::FeedbackFlow::FEEDBACK_FLOW_SADTABCRASH
          : feedback_private::FeedbackFlow::FEEDBACK_FLOW_REGULAR;

  bool include_bluetooth_logs = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (IsGoogleInternalAccount(profile)) {
    flow = feedback_private::FeedbackFlow::FEEDBACK_FLOW_GOOGLEINTERNAL;
    include_bluetooth_logs = IsFromUserInteraction(source);
  }
#endif

  api->RequestFeedbackForFlow(
      description_template, description_placeholder_text, category_tag,
      extra_diagnostics, page_url, flow, source == kFeedbackSourceAssistant,
      include_bluetooth_logs, source == kFeedbackSourceKaleidoscope);
}

}  // namespace

#if BUILDFLAG(IS_CHROMEOS_LACROS)
namespace internal {
// Requests to show Feedback ui remotely in ash via crosapi mojo call.
void ShowFeedbackPageLacros(const GURL& page_url,
                            FeedbackSource source,
                            const std::string& description_template,
                            const std::string& description_placeholder_text,
                            const std::string& category_tag,
                            const std::string& extra_diagnostics);
}  // namespace internal
#endif

void ShowFeedbackPage(const Browser* browser,
                      FeedbackSource source,
                      const std::string& description_template,
                      const std::string& description_placeholder_text,
                      const std::string& category_tag,
                      const std::string& extra_diagnostics) {
  GURL page_url;
  if (browser) {
    page_url = GetTargetTabUrl(browser->session_id(),
                               browser->tab_strip_model()->active_index());
  }

  Profile* profile = GetFeedbackProfile(browser);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // When users invoke the feedback dialog by pressing alt-shift-i without
  // an active ash window, we need to check if there is an active lacros window
  // and show its Url in the feedback dialog if there is any.
  if (!browser && crosapi::BrowserManager::Get()->IsRunning() &&
      crosapi::BrowserManager::Get()->GetActiveTabUrlSupported()) {
    crosapi::BrowserManager::Get()->GetActiveTabUrl(base::BindOnce(
        &OnLacrosActiveTabUrlFeteched, profile, source, description_template,
        description_placeholder_text, category_tag, extra_diagnostics));
  } else {
    ShowFeedbackPage(page_url, profile, source, description_template,
                     description_placeholder_text, category_tag,
                     extra_diagnostics);
  }
#else
  ShowFeedbackPage(page_url, profile, source, description_template,
                   description_placeholder_text, category_tag,
                   extra_diagnostics);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ShowFeedbackPage(const GURL& page_url,
                      Profile* profile,
                      FeedbackSource source,
                      const std::string& description_template,
                      const std::string& description_placeholder_text,
                      const std::string& category_tag,
                      const std::string& extra_diagnostics) {
  if (!profile) {
    LOG(ERROR) << "Cannot invoke feedback: No profile found!";
    return;
  }
  if (!profile->GetPrefs()->GetBoolean(prefs::kUserFeedbackAllowed)) {
    return;
  }
  // Record an UMA histogram to know the most frequent feedback request source.
  UMA_HISTOGRAM_ENUMERATION("Feedback.RequestSource", source,
                            kFeedbackSourceCount);

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  if (chromeos::LacrosChromeServiceImpl::Get()->IsFeedbackAvailable()) {
    // Send request to ash via crosapi mojo to show Feedback ui from ash.
    internal::ShowFeedbackPageLacros(page_url, source, description_template,
                                     description_placeholder_text, category_tag,
                                     extra_diagnostics);
  } else {
    // If ash version is too old, which does not support Feedback crosapi,
    // invoke the Feedback ui from feedback extension in lacros and send
    // a simple lacros feedback report for backward compatibility support.
    // TODO(http://crbug.com/1132106): Remove this code after M87 beta
    // when Feedback should be available in crosapi for all ash versions.
    RequestFeedbackFlow(page_url, profile, source, description_template,
                        description_placeholder_text, category_tag,
                        extra_diagnostics);
  }
#else
  // Show feedback dialog using feedback extension API.
  RequestFeedbackFlow(page_url, profile, source, description_template,
                      description_placeholder_text, category_tag,
                      extra_diagnostics);
#endif  //  BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace chrome
