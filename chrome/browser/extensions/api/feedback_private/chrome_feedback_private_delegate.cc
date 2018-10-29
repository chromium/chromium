// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/feedback_private/chrome_feedback_private_delegate.h"

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feedback/feedback_uploader_chrome.h"
#include "chrome/browser/feedback/feedback_uploader_factory_chrome.h"
#include "chrome/browser/feedback/system_logs/chrome_system_logs_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/grit/generated_resources.h"
#include "components/feedback/system_logs/system_logs_fetcher.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/browser_context.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/chromeos/system_logs/iwlwifi_dump_log_source.h"
#include "chrome/browser/chromeos/system_logs/single_debug_daemon_log_source.h"
#include "chrome/browser/chromeos/system_logs/single_log_file_log_source.h"
#include "chrome/browser/profiles/profile.h"
#include "components/feedback/system_logs/system_logs_source.h"
#endif  // defined(OS_CHROMEOS)

namespace extensions {

ChromeFeedbackPrivateDelegate::ChromeFeedbackPrivateDelegate() = default;
ChromeFeedbackPrivateDelegate::~ChromeFeedbackPrivateDelegate() = default;

std::unique_ptr<base::DictionaryValue>
ChromeFeedbackPrivateDelegate::GetStrings(
    content::BrowserContext* browser_context,
    bool from_crash) const {
  std::unique_ptr<base::DictionaryValue> dict =
      std::make_unique<base::DictionaryValue>();

#define SET_STRING(id, idr) dict->SetString(id, l10n_util::GetStringUTF16(idr))
  SET_STRING("page-title", from_crash
                               ? IDS_FEEDBACK_REPORT_PAGE_TITLE_SAD_TAB_FLOW
                               : IDS_FEEDBACK_REPORT_PAGE_TITLE);
  SET_STRING("additionalInfo", IDS_FEEDBACK_ADDITIONAL_INFO_LABEL);
  SET_STRING("minimize-btn-label", IDS_FEEDBACK_MINIMIZE_BUTTON_LABEL);
  SET_STRING("close-btn-label", IDS_FEEDBACK_CLOSE_BUTTON_LABEL);
  SET_STRING("page-url", IDS_FEEDBACK_REPORT_URL_LABEL);
  SET_STRING("screenshot", IDS_FEEDBACK_SCREENSHOT_LABEL);
  SET_STRING("user-email", IDS_FEEDBACK_USER_EMAIL_LABEL);
  SET_STRING("anonymous-user", IDS_FEEDBACK_ANONYMOUS_EMAIL_OPTION);
#if defined(OS_CHROMEOS)
  if (arc::IsArcPlayStoreEnabledForProfile(
          Profile::FromBrowserContext(browser_context))) {
    SET_STRING("sys-info",
               IDS_FEEDBACK_INCLUDE_SYSTEM_INFORMATION_AND_METRICS_CHKBOX_ARC);
  } else {
    SET_STRING("sys-info",
               IDS_FEEDBACK_INCLUDE_SYSTEM_INFORMATION_AND_METRICS_CHKBOX);
  }
#else
  SET_STRING("sys-info", IDS_FEEDBACK_INCLUDE_SYSTEM_INFORMATION_CHKBOX);
#endif
  SET_STRING("attach-file-label", IDS_FEEDBACK_ATTACH_FILE_LABEL);
  SET_STRING("attach-file-note", IDS_FEEDBACK_ATTACH_FILE_NOTE);
  SET_STRING("attach-file-to-big", IDS_FEEDBACK_ATTACH_FILE_TO_BIG);
  SET_STRING("reading-file", IDS_FEEDBACK_READING_FILE);
  SET_STRING("send-report", IDS_FEEDBACK_SEND_REPORT);
  SET_STRING("cancel", IDS_CANCEL);
  SET_STRING("no-description", IDS_FEEDBACK_NO_DESCRIPTION);
  SET_STRING("privacy-note", IDS_FEEDBACK_PRIVACY_NOTE);
  SET_STRING("performance-trace",
             IDS_FEEDBACK_INCLUDE_PERFORMANCE_TRACE_CHECKBOX);
  SET_STRING("bluetooth-logs-info", IDS_FEEDBACK_BLUETOOTH_LOGS_CHECKBOX);
  SET_STRING("bluetooth-logs-message", IDS_FEEDBACK_BLUETOOTH_LOGS_MESSAGE);
  // Add the localized strings needed for the "system information" page.
  SET_STRING("sysinfoPageTitle", IDS_FEEDBACK_SYSINFO_PAGE_TITLE);
  SET_STRING("sysinfoPageDescription", IDS_ABOUT_SYS_DESC);
  SET_STRING("sysinfoPageTableTitle", IDS_ABOUT_SYS_TABLE_TITLE);
  SET_STRING("sysinfoPageExpandAllBtn", IDS_ABOUT_SYS_EXPAND_ALL);
  SET_STRING("sysinfoPageCollapseAllBtn", IDS_ABOUT_SYS_COLLAPSE_ALL);
  SET_STRING("sysinfoPageExpandBtn", IDS_ABOUT_SYS_EXPAND);
  SET_STRING("sysinfoPageCollapseBtn", IDS_ABOUT_SYS_COLLAPSE);
  SET_STRING("sysinfoPageStatusLoading", IDS_FEEDBACK_SYSINFO_PAGE_LOADING);
  // And the localized strings needed for the SRT Download Prompt.
  SET_STRING("srtPromptBody", IDS_FEEDBACK_SRT_PROMPT_BODY);
  SET_STRING("srtPromptAcceptButton", IDS_FEEDBACK_SRT_PROMPT_ACCEPT_BUTTON);
  SET_STRING("srtPromptDeclineButton", IDS_FEEDBACK_SRT_PROMPT_DECLINE_BUTTON);
#undef SET_STRING

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, dict.get());

  return dict;
}

system_logs::SystemLogsFetcher*
ChromeFeedbackPrivateDelegate::CreateSystemLogsFetcher(
    content::BrowserContext* context) const {
  return system_logs::BuildChromeSystemLogsFetcher();
}

#if defined(OS_CHROMEOS)
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

void ChromeFeedbackPrivateDelegate::FetchAndMergeIwlwifiDumpLogsIfPresent(
    std::unique_ptr<FeedbackCommon::SystemLogsMap> original_sys_logs,
    content::BrowserContext* context,
    system_logs::SysLogsFetcherCallback callback) const {
  if (!original_sys_logs ||
      !system_logs::ContainsIwlwifiLogs(original_sys_logs.get())) {
    std::move(callback).Run(std::move(original_sys_logs));
    return;
  }

  system_logs::SystemLogsFetcher* fetcher =
      new system_logs::SystemLogsFetcher(true /* scrub_data */);
  fetcher->AddSource(std::make_unique<system_logs::IwlwifiDumpLogSource>());
  fetcher->Fetch(base::BindOnce(&system_logs::MergeIwlwifiLogs,
                                std::move(original_sys_logs),
                                std::move(callback)));
}
#endif  // defined(OS_CHROMEOS)

std::string ChromeFeedbackPrivateDelegate::GetSignedInUserEmail(
    content::BrowserContext* context) const {
  auto* identity_manager = IdentityManagerFactory::GetForProfile(
      Profile::FromBrowserContext(context));
  return identity_manager ? identity_manager->GetPrimaryAccountInfo().email
                          : std::string();
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

}  // namespace extensions
