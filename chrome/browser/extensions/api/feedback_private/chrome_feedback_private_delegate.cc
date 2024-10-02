// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/feedback_private/chrome_feedback_private_delegate.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/feedback/feedback_uploader_chrome.h"
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/feedback/system_logs/chrome_system_logs_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/generated_resources.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/extensions_browser_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/system_logs/iwlwifi_dump_log_source.h"
#include "chrome/browser/ash/system_logs/single_debug_daemon_log_source.h"
#include "chrome/browser/ash/system_logs/single_log_file_log_source.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "components/feedback/system_logs/system_logs_source.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/constants.h"
#include "google_apis/gaia/gaia_auth_util.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

namespace {

int GetSysInfoCheckboxStringId(content::BrowserContext* browser_context) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (arc::IsArcPlayStoreEnabledForProfile(
          Profile::FromBrowserContext(browser_context))) {
    return IDS_FEEDBACK_INCLUDE_SYSTEM_INFORMATION_AND_METRICS_CHKBOX_ARC;
  } else {
    return IDS_FEEDBACK_INCLUDE_SYSTEM_INFORMATION_AND_METRICS_CHKBOX;
  }
#else
  return IDS_FEEDBACK_INCLUDE_SYSTEM_INFORMATION_CHKBOX;
#endif
}

}  // namespace

ChromeFeedbackPrivateDelegate::ChromeFeedbackPrivateDelegate() = default;
ChromeFeedbackPrivateDelegate::~ChromeFeedbackPrivateDelegate() = default;

base::Value::Dict ChromeFeedbackPrivateDelegate::GetStrings(
    content::BrowserContext* browser_context,
    bool from_crash) const {
  base::Value::Dict dict;

#define SET_STRING(id, idr) dict.Set(id, l10n_util::GetStringUTF16(idr))
  SET_STRING("pageTitle", from_crash
                              ? IDS_FEEDBACK_REPORT_PAGE_TITLE_SAD_TAB_FLOW
                              : IDS_FEEDBACK_REPORT_PAGE_TITLE);
  SET_STRING("appTitle", IDS_FEEDBACK_REPORT_APP_TITLE);
  SET_STRING("additionalInfo", IDS_FEEDBACK_ADDITIONAL_INFO_LABEL);
  SET_STRING("minimizeBtnLabel", IDS_FEEDBACK_MINIMIZE_BUTTON_LABEL);
  SET_STRING("closeBtnLabel", IDS_FEEDBACK_CLOSE_BUTTON_LABEL);
  SET_STRING("freeFormText", IDS_FEEDBACK_FREE_TEXT_LABEL);
  SET_STRING("pageUrl", IDS_FEEDBACK_REPORT_URL_LABEL);
  SET_STRING("screenshot", IDS_FEEDBACK_SCREENSHOT_LABEL);
  SET_STRING("screenshotA11y", IDS_FEEDBACK_SCREENSHOT_A11Y_TEXT);
  SET_STRING("userEmail", IDS_FEEDBACK_USER_EMAIL_LABEL);
  SET_STRING("anonymousUser", IDS_FEEDBACK_ANONYMOUS_EMAIL_OPTION);
  SET_STRING("sysInfo", GetSysInfoCheckboxStringId(browser_context));
  SET_STRING("attachFileLabel", IDS_FEEDBACK_ATTACH_FILE_LABEL);
  SET_STRING("attachFileNote", IDS_FEEDBACK_ATTACH_FILE_NOTE);
  SET_STRING("attachFileToBig", IDS_FEEDBACK_ATTACH_FILE_TO_BIG);
  SET_STRING("sendReport", IDS_FEEDBACK_SEND_REPORT);
  SET_STRING("cancel", IDS_CANCEL);
  SET_STRING("noDescription", IDS_FEEDBACK_NO_DESCRIPTION);
  SET_STRING("privacyNote", IDS_FEEDBACK_PRIVACY_NOTE);

  // Add the localized strings needed for the "system information" page.
  SET_STRING("sysinfoPageTitle", IDS_FEEDBACK_SYSINFO_PAGE_TITLE);
  SET_STRING("sysinfoPageDescription", IDS_ABOUT_SYS_DESC);

  // Add the localized strings shared by the "autofill metadata" and "system
  // information" page.
  SET_STRING("logsMapPageTableTitle", IDS_ABOUT_SYS_TABLE_TITLE);
  SET_STRING("logsMapPageExpandAllBtn", IDS_ABOUT_SYS_EXPAND_ALL);
  SET_STRING("logsMapPageCollapseAllBtn", IDS_ABOUT_SYS_COLLAPSE_ALL);
  SET_STRING("logsMapPageExpandBtn", IDS_ABOUT_SYS_EXPAND);
  SET_STRING("logsMapPageCollapseBtn", IDS_ABOUT_SYS_COLLAPSE);
  SET_STRING("logsMapPageStatusLoading", IDS_FEEDBACK_SYSINFO_PAGE_LOADING);
#undef SET_STRING

  std::string app_locale =
      ExtensionsBrowserClient::Get()->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &dict);

  return dict;
}

void ChromeFeedbackPrivateDelegate::FetchSystemInformation(
    content::BrowserContext* context,
    system_logs::SysLogsFetcherCallback callback) const {
  // self-deleting object
  auto* fetcher = system_logs::BuildChromeSystemLogsFetcher(
      Profile::FromBrowserContext(context), /*scrub_data=*/true);
  fetcher->Fetch(std::move(callback));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
std::unique_ptr<system_logs::SystemLogsSource>
ChromeFeedbackPrivateDelegate::CreateSingleLogSource(
    api::feedback_private::LogSource source_type) const {
  using SupportedLogFileSource =
      system_logs::SingleLogFileLogSource::SupportedSource;
  using SupportedDebugDaemonSource =
      system_logs::SingleDebugDaemonLogSource::SupportedSource;

  switch (source_type) {
    // These map to SupportedLogFileSources.
    case api::feedback_private::LogSource::kMessages:
      return std::make_unique<system_logs::SingleLogFileLogSource>(
          SupportedLogFileSource::kMessages);
    case api::feedback_private::LogSource::kUiLatest:
      return std::make_unique<system_logs::SingleLogFileLogSource>(
          SupportedLogFileSource::kUiLatest);
    case api::feedback_private::LogSource::kAtrusLog:
      return std::make_unique<system_logs::SingleLogFileLogSource>(
          SupportedLogFileSource::kAtrusLog);
    case api::feedback_private::LogSource::kNetLog:
      return std::make_unique<system_logs::SingleLogFileLogSource>(
          SupportedLogFileSource::kNetLog);
    case api::feedback_private::LogSource::kEventLog:
      return std::make_unique<system_logs::SingleLogFileLogSource>(
          SupportedLogFileSource::kEventLog);
    case api::feedback_private::LogSource::kUpdateEngineLog:
      return std::make_unique<system_logs::SingleLogFileLogSource>(
          SupportedLogFileSource::kUpdateEngineLog);
    case api::feedback_private::LogSource::kPowerdLatest:
      return std::make_unique<system_logs::SingleLogFileLogSource>(
          SupportedLogFileSource::kPowerdLatest);
    case api::feedback_private::LogSource::kPowerdPrevious:
      return std::make_unique<system_logs::SingleLogFileLogSource>(
          SupportedLogFileSource::kPowerdPrevious);

    // These map to SupportedDebugDaemonSources.
    case api::feedback_private::LogSource::kDrmModetest:
      return std::make_unique<system_logs::SingleDebugDaemonLogSource>(
          SupportedDebugDaemonSource::kModetest);
    case api::feedback_private::LogSource::kLsusb:
      return std::make_unique<system_logs::SingleDebugDaemonLogSource>(
          SupportedDebugDaemonSource::kLsusb);
    case api::feedback_private::LogSource::kLspci:
      return std::make_unique<system_logs::SingleDebugDaemonLogSource>(
          SupportedDebugDaemonSource::kLspci);
    case api::feedback_private::LogSource::kIfconfig:
      return std::make_unique<system_logs::SingleDebugDaemonLogSource>(
          SupportedDebugDaemonSource::kIfconfig);
    case api::feedback_private::LogSource::kUptime:
      return std::make_unique<system_logs::SingleDebugDaemonLogSource>(
          SupportedDebugDaemonSource::kUptime);

    case api::feedback_private::LogSource::kNone:
    default:
      NOTREACHED_IN_MIGRATION() << "Unknown log source type.";
      return nullptr;
  }
}

void OnFetchedExtraLogs(
    scoped_refptr<feedback::FeedbackData> feedback_data,
    FetchExtraLogsCallback callback,
    std::unique_ptr<system_logs::SystemLogsResponse> response) {
  using system_logs::kIwlwifiDumpKey;
  if (response && response->count(kIwlwifiDumpKey)) {
    feedback_data->AddLog(kIwlwifiDumpKey,
                          std::move(response->at(kIwlwifiDumpKey)));
  }
  std::move(callback).Run(feedback_data);
}

void ChromeFeedbackPrivateDelegate::FetchExtraLogs(
    scoped_refptr<feedback::FeedbackData> feedback_data,
    FetchExtraLogsCallback callback) const {
  // Anonymize data.
  constexpr bool scrub = true;

  if (system_logs::ContainsIwlwifiLogs(feedback_data->sys_info())) {
    // TODO (jkardatzke): Modify this so that we are using the same instance of
    // the anonymizer for the rest of the logs.
    // We can pass null for the 1st party IDs since we are just anonymizing
    // wifi data here.
    system_logs::SystemLogsFetcher* fetcher =
        new system_logs::SystemLogsFetcher(scrub, nullptr);
    fetcher->AddSource(std::make_unique<system_logs::IwlwifiDumpLogSource>());
    fetcher->Fetch(base::BindOnce(&OnFetchedExtraLogs, feedback_data,
                                  std::move(callback)));
  } else {
    std::move(callback).Run(feedback_data);
  }
}

api::feedback_private::LandingPageType
ChromeFeedbackPrivateDelegate::GetLandingPageType(
    const feedback::FeedbackData& feedback_data) const {
  // Googlers using eve get a custom landing page.
  if (!gaia::IsGoogleInternalAccountEmail(feedback_data.user_email()))
    return api::feedback_private::LandingPageType::kNormal;

  const std::vector<std::string> board =
      base::SplitString(base::SysInfo::GetLsbReleaseBoard(), "-",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return board[0] == "eve" ? api::feedback_private::LandingPageType::kTechstop
                           : api::feedback_private::LandingPageType::kNormal;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

std::string ChromeFeedbackPrivateDelegate::GetSignedInUserEmail(
    content::BrowserContext* context) const {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(
      Profile::FromBrowserContext(context));
  if (!identity_manager)
    return std::string();
  // Browser sync consent is not required to use feedback.
  return identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
      .email;
}

void ChromeFeedbackPrivateDelegate::NotifyFeedbackDelayed() const {
  // Show a message box to indicate that sending the feedback has been delayed
  // because the user is offline.
  chrome::ShowWarningMessageBox(
      nullptr, l10n_util::GetStringUTF16(IDS_FEEDBACK_OFFLINE_DIALOG_TITLE),
      l10n_util::GetStringUTF16(IDS_FEEDBACK_OFFLINE_DIALOG_TEXT));
}

feedback::FeedbackUploader*
ChromeFeedbackPrivateDelegate::GetFeedbackUploaderForContext(
    content::BrowserContext* context) const {
  return feedback::FeedbackUploaderFactoryChrome::GetForBrowserContext(context);
}

void ChromeFeedbackPrivateDelegate::OpenFeedback(
    content::BrowserContext* context,
    api::feedback_private::FeedbackSource source) const {
  GURL url;

  DCHECK(source == api::feedback_private::FeedbackSource::kQuickoffice);

  Profile* profile = Profile::FromBrowserContext(context);
  chrome::ShowFeedbackPage(url, profile,
                           /*source=*/feedback::kFeedbackSourceQuickOffice,
                           /*description_template=*/std::string(),
                           /*description_placeholder_text=*/std::string(),
                           /*category_tag=*/std::string(),
                           /*extra_diagnostics=*/std::string());
}

}  // namespace extensions
