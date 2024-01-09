// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/json/json_writer.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/feedback/feedback_dialog_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "extensions/browser/api/feedback_private/feedback_private_api.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_features.h"
#include "ash/webui/os_feedback_ui/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "google_apis/gaia/gaia_auth_util.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/feedback.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

namespace feedback_private = extensions::api::feedback_private;

namespace chrome {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
constexpr char kExtraDiagnosticsQueryParam[] = "extra_diagnostics";
constexpr char kDescriptionTemplateQueryParam[] = "description_template";
constexpr char kDescriptionPlaceholderQueryParam[] =
    "description_placeholder_text";
constexpr char kFromAssistantQueryParam[] = "from_assistant";
constexpr char kSettingsSearchFeedbackQueryParam[] = "from_settings_search";
constexpr char kCategoryTagParam[] = "category_tag";
constexpr char kPageURLParam[] = "page_url";
constexpr char kQueryParamSeparator[] = "&";
constexpr char kQueryParamKeyValueSeparator[] = "=";
constexpr char kFromAssistantQueryParamValue[] = "true";
constexpr char kSettingsSearchFeedbackQueryParamValue[] = "true";
constexpr char kFromAutofillQueryParam[] = "from_autofill";
constexpr char kFromAutofillParamValue[] = "true";
constexpr char kAutofillMetadataQueryParam[] = "autofill_metadata";

// Concat query parameter with escaped value.
std::string StrCatQueryParam(const std::string query_param,
                             const std::string value) {
  return base::StrCat({query_param, kQueryParamKeyValueSeparator,
                       base::EscapeQueryParamValue(value, /*use_plus=*/false)});
}

// Returns URL for OS Feedback with additional data passed as query parameters.
GURL BuildFeedbackUrl(const std::string extra_diagnostics,
                      const std::string description_template,
                      const std::string description_placeholder_text,
                      const std::string category_tag,
                      const GURL page_url,
                      FeedbackSource source,
                      base::Value::Dict autofill_metadata) {
  std::vector<std::string> query_params;

  if (!extra_diagnostics.empty()) {
    query_params.emplace_back(
        StrCatQueryParam(kExtraDiagnosticsQueryParam, extra_diagnostics));
  }

  if (!description_template.empty()) {
    query_params.emplace_back(
        StrCatQueryParam(kDescriptionTemplateQueryParam, description_template));
  }

  if (!description_placeholder_text.empty()) {
    query_params.emplace_back(StrCatQueryParam(
        kDescriptionPlaceholderQueryParam, description_placeholder_text));
  }

  if (!category_tag.empty()) {
    query_params.emplace_back(
        StrCatQueryParam(kCategoryTagParam, category_tag));
  }

  if (!page_url.is_empty()) {
    query_params.emplace_back(StrCatQueryParam(kPageURLParam, page_url.spec()));
  }

  if (source == kFeedbackSourceAssistant) {
    query_params.emplace_back(StrCatQueryParam(kFromAssistantQueryParam,
                                               kFromAssistantQueryParamValue));
  }

  if (source == kFeedbackSourceOsSettingsSearch) {
    query_params.emplace_back(
        StrCatQueryParam(kSettingsSearchFeedbackQueryParam,
                         kSettingsSearchFeedbackQueryParamValue));
  }

  if (source == kFeedbackSourceAutofillContextMenu) {
    query_params.emplace_back(
        StrCatQueryParam(kFromAutofillQueryParam, kFromAutofillParamValue));

    std::string autofill_metadata_json;
    base::JSONWriter::Write(autofill_metadata, &autofill_metadata_json);
    query_params.emplace_back(
        StrCatQueryParam(kAutofillMetadataQueryParam, autofill_metadata_json));
  }

  // Use default URL if no extra parameters to be added.
  if (query_params.empty()) {
    return GURL(ash::kChromeUIOSFeedbackUrl);
  }

  return GURL(
      base::StrCat({ash::kChromeUIOSFeedbackUrl, "/?",
                    base::JoinString(query_params, kQueryParamSeparator)}));
}

// Returns whether the user has an internal Google account (e.g. @google.com).
bool IsGoogleInternalAccount(Profile* profile) {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);
  if (!identity_manager)  // Non-GAIA account, e.g. guest mode.
    return false;
  // Browser sync consent is not required to use feedback.
  CoreAccountInfo account_info =
      identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin);
  return gaia::IsGoogleInternalAccountEmail(account_info.email);
}

// Returns if the feedback page is considered to be triggered from user
// interaction.
bool IsFromUserInteraction(FeedbackSource source) {
  switch (source) {
    case kFeedbackSourceArcApp:
    case kFeedbackSourceAsh:
    case kFeedbackSourceAssistant:
    case kFeedbackSourceAutofillContextMenu:
    case kFeedbackSourceBrowserCommand:
    case kFeedbackSourceConnectivityDiagnostics:
    case kFeedbackSourceDesktopTabGroups:
    case kFeedbackSourceCookieControls:
    case kFeedbackSourceNetworkHealthPage:
    case kFeedbackSourceMdSettingsAboutPage:
    case kFeedbackSourceOldSettingsAboutPage:
    case kFeedbackSourceOsSettingsSearch:
    case kFeedbackSourcePriceInsights:
    case kFeedbackSourceQuickAnswers:
    case kFeedbackSourceQuickOffice:
    case kFeedbackSourceSettingsPerformancePage:
      return true;
    default:
      return false;
  }
}

void OnLacrosActiveTabUrlFetched(
    Profile* profile,
    chrome::FeedbackSource source,
    const std::string& description_template,
    const std::string& description_placeholder_text,
    const std::string& category_tag,
    const std::string& extra_diagnostics,
    base::Value::Dict autofill_metadata,
    base::Value::Dict ai_metadata,
    const absl::optional<GURL>& active_tab_url) {
  GURL page_url;
  if (active_tab_url)
    page_url = *active_tab_url;
  chrome::ShowFeedbackPage(page_url, profile, source, description_template,
                           description_placeholder_text, category_tag,
                           extra_diagnostics, std::move(autofill_metadata),
                           std::move(ai_metadata));
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if !BUILDFLAG(IS_CHROMEOS_LACROS)

feedback_private::FeedbackFlow GetFeedbackFlowFromSource(
    FeedbackSource source) {
  switch (source) {
    case kFeedbackSourceSadTabPage:
      return feedback_private::FeedbackFlow::kSadTabCrash;
    case kFeedbackSourceAutofillContextMenu:
      return feedback_private::FeedbackFlow::kGoogleInternal;
    case kFeedbackSourceAI:
      return feedback_private::FeedbackFlow::kAi;
    default:
      return feedback_private::FeedbackFlow::kRegular;
  }
}

// Calls feedback private api to show Feedback ui.
void RequestFeedbackFlow(const GURL& page_url,
                         Profile* profile,
                         FeedbackSource source,
                         const std::string& description_template,
                         const std::string& description_placeholder_text,
                         const std::string& category_tag,
                         const std::string& extra_diagnostics,
                         base::Value::Dict autofill_metadata,
                         base::Value::Dict ai_metadata) {
  feedback_private::FeedbackFlow flow = GetFeedbackFlowFromSource(source);
  bool include_bluetooth_logs = false;
  bool show_questionnaire = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/1501057) Support ChromeOS feedback dialog for
  // `kFeedbackSourceAI`.
  if (source != kFeedbackSourceAI) {
    if (IsGoogleInternalAccount(profile)) {
      flow = feedback_private::FeedbackFlow::kGoogleInternal;
      include_bluetooth_logs = IsFromUserInteraction(source);
      show_questionnaire = IsFromUserInteraction(source);
    }
    // Disable the new feedback tool for kiosk, when SWAs are disabled there.
    if (!chromeos::IsKioskSession() ||
        base::FeatureList::IsEnabled(
            ash::features::kKioskEnableSystemWebApps)) {
      // TODO(crbug.com/1407646): Include autofill metadata into CrOS new
      // feedback tool.
      ash::SystemAppLaunchParams params;
      params.url = BuildFeedbackUrl(
          extra_diagnostics, description_template, description_placeholder_text,
          category_tag, page_url, source, std::move(autofill_metadata));
      ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::OS_FEEDBACK,
                                   std::move(params));
      return;
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  extensions::FeedbackPrivateAPI* api =
      extensions::FeedbackPrivateAPI::GetFactoryInstance()->Get(profile);
  auto info = api->CreateFeedbackInfo(
      description_template, description_placeholder_text, category_tag,
      extra_diagnostics, page_url, flow, source == kFeedbackSourceAssistant,
      include_bluetooth_logs, show_questionnaire,
      source == kFeedbackSourceChromeLabs ||
          source == kFeedbackSourceKaleidoscope,
      source == kFeedbackSourceAutofillContextMenu, autofill_metadata,
      ai_metadata);

  FeedbackDialog::CreateOrShow(profile, *info);
}
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

#if BUILDFLAG(IS_CHROMEOS_LACROS)
namespace internal {
// Requests to show Feedback ui remotely in ash via crosapi mojo call.
void ShowFeedbackPageLacros(const GURL& page_url,
                            FeedbackSource source,
                            const std::string& description_template,
                            const std::string& description_placeholder_text,
                            const std::string& category_tag,
                            const std::string& extra_diagnostics,
                            base::Value::Dict autofill_metadata,
                            base::Value::Dict ai_metadata);
}  // namespace internal
#endif

void ShowFeedbackPage(const Browser* browser,
                      FeedbackSource source,
                      const std::string& description_template,
                      const std::string& description_placeholder_text,
                      const std::string& category_tag,
                      const std::string& extra_diagnostics,
                      base::Value::Dict autofill_metadata,
                      base::Value::Dict ai_metadata) {
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
        &OnLacrosActiveTabUrlFetched, profile, source, description_template,
        description_placeholder_text, category_tag, extra_diagnostics,
        std::move(autofill_metadata), std::move(ai_metadata)));
  } else {
    ShowFeedbackPage(page_url, profile, source, description_template,
                     description_placeholder_text, category_tag,
                     extra_diagnostics, std::move(autofill_metadata),
                     std::move(ai_metadata));
  }
#else
  ShowFeedbackPage(page_url, profile, source, description_template,
                   description_placeholder_text, category_tag,
                   extra_diagnostics, std::move(autofill_metadata),
                   std::move(ai_metadata));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

void ShowFeedbackPage(const GURL& page_url,
                      Profile* profile,
                      FeedbackSource source,
                      const std::string& description_template,
                      const std::string& description_placeholder_text,
                      const std::string& category_tag,
                      const std::string& extra_diagnostics,
                      base::Value::Dict autofill_metadata,
                      base::Value::Dict ai_metadata) {
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
  // TODO(crbug.com/1501057): Update enhanced feedback dialog crosapi API for
  // Lacros.
  // After M87 beta, Feedback API should be supported in crosapi with
  // all ash versions on chromeOS platform where lacros is deployed.
  DCHECK(
      chromeos::LacrosService::Get()->IsAvailable<crosapi::mojom::Feedback>());
  // Send request to ash via crosapi mojo to show Feedback ui from ash.
  internal::ShowFeedbackPageLacros(
      page_url, source, description_template, description_placeholder_text,
      category_tag, extra_diagnostics, std::move(autofill_metadata),
      std::move(ai_metadata));
#else
  // Show feedback dialog using feedback extension API.
  RequestFeedbackFlow(page_url, profile, source, description_template,
                      description_placeholder_text, category_tag,
                      extra_diagnostics, std::move(autofill_metadata),
                      std::move(ai_metadata));
#endif  //  BUILDFLAG(IS_CHROMEOS_LACROS)
}

}  // namespace chrome
