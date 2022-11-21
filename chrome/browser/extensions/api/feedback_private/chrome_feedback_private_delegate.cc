// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/feedback_private/chrome_feedback_private_delegate.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feedback/feedback_uploader_chrome.h"
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/feedback/system_logs/chrome_system_logs_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/generated_resources.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/strings/string_split.h"
#include "base/system/sys_info.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/crosapi/browser_manager.h"
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
  SET_STRING("assistantInfo",
             IDS_FEEDBACK_INCLUDE_ASSISTANT_INFORMATION_CHKBOX);
  SET_STRING("attachFileLabel", IDS_FEEDBACK_ATTACH_FILE_LABEL);
  SET_STRING("attachFileNote", IDS_FEEDBACK_ATTACH_FILE_NOTE);
  SET_STRING("attachFileToBig", IDS_FEEDBACK_ATTACH_FILE_TO_BIG);
  SET_STRING("sendReport", IDS_FEEDBACK_SEND_REPORT);
  SET_STRING("cancel", IDS_CANCEL);
  SET_STRING("noDescription", IDS_FEEDBACK_NO_DESCRIPTION);
  SET_STRING("privacyNote", IDS_FEEDBACK_PRIVACY_NOTE);
  SET_STRING("performanceTrace",
             IDS_FEEDBACK_INCLUDE_PERFORMANCE_TRACE_CHECKBOX);
  SET_STRING("bluetoothLogsInfo", IDS_FEEDBACK_BLUETOOTH_LOGS_CHECKBOX);
  SET_STRING("bluetoothLogsMessage", IDS_FEEDBACK_BLUETOOTH_LOGS_MESSAGE);
  SET_STRING("assistantLogsMessage", IDS_FEEDBACK_ASSISTANT_LOGS_MESSAGE);

  // Add the localized strings needed for the "system information" page.
  SET_STRING("sysinfoPageTitle", IDS_FEEDBACK_SYSINFO_PAGE_TITLE);
  SET_STRING("sysinfoPageDescription", IDS_ABOUT_SYS_DESC);
  SET_STRING("sysinfoPageTableTitle", IDS_ABOUT_SYS_TABLE_TITLE);
  SET_STRING("sysinfoPageExpandAllBtn", IDS_ABOUT_SYS_EXPAND_ALL);
  SET_STRING("sysinfoPageCollapseAllBtn", IDS_ABOUT_SYS_COLLAPSE_ALL);
  SET_STRING("sysinfoPageExpandBtn", IDS_ABOUT_SYS_EXPAND);
  SET_STRING("sysinfoPageCollapseBtn", IDS_ABOUT_SYS_COLLAPSE);
  SET_STRING("sysinfoPageStatusLoading", IDS_FEEDBACK_SYSINFO_PAGE_LOADING);
#undef SET_STRING

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, &dict);

  return dict;
}

void ChromeFeedbackPrivateDelegate::FetchSystemInformation(
    content::BrowserContext* context,
    system_logs::SysLogsFetcherCallback callback) const {
  // self-deleting object
  auto* fetcher =
      system_logs::BuildChromeSystemLogsFetcher(/*scrub_data=*/true);
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
    case api::feedback_private::LOG_SOURCE_MESSAGES:
      return std::make_unique<system_logs::SingleLogFileLogSource>(
          SupportedLogFileSource::kMessages);
    case api::feedback_private::LOG_SOURCE_UILATEST:
      return std::make_unique<system_logs::SingleLogFileLogSource>(
          SupportedLogFileSource::kUiLatest);
    case api::feedback_private::LOG_SOURCE_ATRUSLOG:
      return std::make_unique<system_logs::SingleLogFileLogSource>(
          SupportedLogFileSource::kAtrusLog);
    case api::feedback_private::LOG_SOURCE_NETLOG:
      return std::make_unique<system_logs::SingleLogFileLogSource>(
          SupportedLogFileSource::kNetLog);
    case api::feedback_private::LOG_SOURCE_EVENTLOG:
      return std::make_unique<system_logs::SingleLogFileLogSource>(
          SupportedLogFileSource::kEventLog);
    case api::feedback_private::LOG_SOURCE_UPDATEENGINELOG:
      return std::make_unique<system_logs::SingleLogFileLogSource>(
          SupportedLogFileSource::kUpdateEngineLog);
    case api::feedback_private::LOG_SOURCE_POWERDLATEST:
      return std::make_unique<system_logs::SingleLogFileLogSource>(
          SupportedLogFileSource::kPowerdLatest);
    case api::feedback_private::LOG_SOURCE_POWERDPREVIOUS:
      return std::make_unique<system_logs::SingleLogFileLogSource>(
          SupportedLogFileSource::kPowerdPrevious);

    // These map to SupportedDebugDaemonSources.
    case api::feedback_private::LOG_SOURCE_DRMMODETEST:
      return std::make_unique<system_logs::SingleDebugDaemonLogSource>(
          SupportedDebugDaemonSource::kModetest);
    case api::feedback_private::LOG_SOURCE_LSUSB:
      return std::make_unique<system_logs::SingleDebugDaemonLogSource>(
          SupportedDebugDaemonSource::kLsusb);
    case api::feedback_private::LOG_SOURCE_LSPCI:
      return std::make_unique<system_logs::SingleDebugDaemonLogSource>(
          SupportedDebugDaemonSource::kLspci);
    case api::feedback_private::LOG_SOURCE_IFCONFIG:
      return std::make_unique<system_logs::SingleDebugDaemonLogSource>(
          SupportedDebugDaemonSource::kIfconfig);
    case api::feedback_private::LOG_SOURCE_UPTIME:
      return std::make_unique<system_logs::SingleDebugDaemonLogSource>(
          SupportedDebugDaemonSource::kUptime);

    case api::feedback_private::LOG_SOURCE_NONE:
    default:
      NOTREACHED() << "Unknown log source type.";
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
    return api::feedback_private::LANDING_PAGE_TYPE_NORMAL;

  const std::vector<std::string> board =
      base::SplitString(base::SysInfo::GetLsbReleaseBoard(), "-",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  return board[0] == "eve" ? api::feedback_private::LANDING_PAGE_TYPE_TECHSTOP
                           : api::feedback_private::LANDING_PAGE_TYPE_NORMAL;
}

void ChromeFeedbackPrivateDelegate::GetLacrosHistograms(
    GetHistogramsCallback callback) {
  crosapi::BrowserManager* browser_manager = crosapi::BrowserManager::Get();
  if (browser_manager->GetHistogramsSupported() &&
      browser_manager->IsRunning()) {
    browser_manager->GetHistograms(std::move(callback));
  } else {
    std::move(callback).Run(std::string());
  }
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

  DCHECK(source ==
         api::feedback_private::FeedbackSource::FEEDBACK_SOURCE_QUICKOFFICE);

  Profile* profile = Profile::FromBrowserContext(context);
  chrome::ShowFeedbackPage(url, profile,
                           /*source=*/chrome::kFeedbackSourceQuickOffice,
                           /*description_template=*/std::string(),
                           /*description_placeholder_text=*/std::string(),
                           /*category_tag=*/std::string(),
                           /*extra_diagnostics=*/std::string());
}

}  // namespace extensions
