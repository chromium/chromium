// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/error_reporting/chrome_js_error_report_processor.h"

#include <tuple>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"
#include "components/crash/core/app/client_upload_info.h"
#include "components/crash/core/app/crashpad.h"
#include "components/feedback/redaction_tool.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/upload_list/crash_upload_list.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/escape.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

constexpr char kCrashEndpointUrl[] = "https://clients2.google.com/cr/report";
constexpr char kCrashEndpointStagingUrl[] =
    "https://clients2.google.com/cr/staging_report";
constexpr char kNoBrowserNoWindow[] = "NO_BROWSER";
constexpr char kRegularTabbedWindow[] = "REGULAR_TABBED";
constexpr char kWebAppWindow[] = "WEB_APP";
constexpr char kSystemWebAppWindow[] = "SYSTEM_WEB_APP";

// Sometimes, the stack trace will contain an error message as the first line,
// which confuses the Crash server. This function deletes it if it is present.
void RemoveErrorMessageFromStackTrace(const std::string& error_message,
                                      std::string& stack_trace) {
  // Keep the original stack trace if the error message is not present.
  const auto error_message_index = stack_trace.find(error_message);
  if (error_message_index == std::string::npos) {
    return;
  }

  // If the stack trace only contains one line, then delete the whole trace.
  const auto first_line_end_index = stack_trace.find('\n');
  if (first_line_end_index == std::string::npos) {
    stack_trace.clear();
    return;
  }

  // Otherwise, delete the first line.
  stack_trace = stack_trace.substr(first_line_end_index + 1);
}

std::string RedactErrorMessage(const std::string& message) {
  return feedback::RedactionTool(/*first_party_extension_ids=*/nullptr)
      .Redact(message);
}

using ParameterMap = std::map<std::string, std::string>;

std::string BuildPostRequestQueryString(const ParameterMap& params) {
  std::vector<std::string> query_parts;
  for (const auto& kv : params) {
    query_parts.push_back(base::StrCat(
        {kv.first, "=",
         net::EscapeQueryParamValue(kv.second, /*use_plus=*/false)}));
  }
  return base::JoinString(query_parts, "&");
}

std::string MapWindowTypeToString(WindowType window_type) {
  switch (window_type) {
    case WindowType::kRegularTabbed:
      return kRegularTabbedWindow;
    case WindowType::kWebApp:
      return kWebAppWindow;
    case WindowType::kSystemWebApp:
      return kSystemWebAppWindow;
    default:
      return kNoBrowserNoWindow;
  }
}

}  // namespace

ChromeJsErrorReportProcessor::ChromeJsErrorReportProcessor()
    : clock_(base::DefaultClock::GetInstance()) {}
ChromeJsErrorReportProcessor::~ChromeJsErrorReportProcessor() = default;

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
void ChromeJsErrorReportProcessor::UpdateReportDatabase(
    std::string remote_report_id,
    base::Time report_time) {
  // Uploads.log format is "seconds_since_epoch,crash_id\n"
  base::FilePath crash_dir_path;
  if (!base::PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dir_path)) {
    VLOG(1) << "Nowhere to write uploads.log";
    return;
  }
  base::FilePath upload_log_path =
      crash_dir_path.AppendASCII(CrashUploadList::kReporterLogFilename);
  base::File upload_log(upload_log_path,
                        base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
  if (!upload_log.IsValid()) {
    VLOG(1) << "Could not open upload.log: "
            << base::File::ErrorToString(upload_log.error_details());
    return;
  }
  std::string line = base::StrCat({base::NumberToString(report_time.ToTimeT()),
                                   ",", remote_report_id, "\n"});
  // WriteAtCurrentPos because O_APPEND.
  if (upload_log.WriteAtCurrentPos(line.c_str(), line.length()) !=
      static_cast<int>(line.length())) {
    VLOG(1) << "Could not write to upload.log";
    return;
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)

void ChromeJsErrorReportProcessor::OnRequestComplete(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    base::ScopedClosureRunner callback_runner,
    base::Time report_time,
    std::unique_ptr<std::string> response_body) {
  if (response_body) {
    VLOG(1) << "Uploaded crash report. ID: " << *response_body;
    // On Chrome OS, we use a different format than other platforms. Since we
    // will soon not call this function at all on Chrome OS (crbug.com/986166),
    // don't bother writing code to write to that format.
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&ChromeJsErrorReportProcessor::UpdateReportDatabase,
                       this, *response_body, report_time),
        callback_runner.Release());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH) && !BUILDFLAG(IS_CHROMEOS_LACROS)
  } else {
    LOG(ERROR) << "Failed to upload crash report";
  }
  // callback_runner may implicitly run the callback when we reach this line if
  // we didn't add a task to update the report database.
}

// Returns the redacted, fixed-up error report if the user consented to have it
// sent. Returns base::nullopt if the user did not consent or we otherwise
// should not send the report. All the MayBlock work should be done in here.
base::Optional<JavaScriptErrorReport>
ChromeJsErrorReportProcessor::CheckConsentAndRedact(
    JavaScriptErrorReport error_report) {
  if (!crash_reporter::GetClientCollectStatsConsent()) {
    return base::nullopt;
  }

  // Remove error message from stack trace before redaction, since redaction
  // might change the error message enough that we don't find it.
  if (error_report.stack_trace) {
    RemoveErrorMessageFromStackTrace(error_report.message,
                                     *error_report.stack_trace);
  }

  error_report.message = RedactErrorMessage(error_report.message);
  // TODO(https://crbug.com/1121816): Also redact stack trace, but don't
  // completely remove the URL (only query & fragment).
  return error_report;
}

struct ChromeJsErrorReportProcessor::PlatformInfo {
  std::string product_name;
  std::string version;
  std::string channel;
  std::string os_version;
};

ChromeJsErrorReportProcessor::PlatformInfo
ChromeJsErrorReportProcessor::GetPlatformInfo() {
  PlatformInfo info;

  // TODO(https://crbug.com/1121816): Get correct product_name for non-POSIX
  // platforms.
#if defined(OS_POSIX)
  crash_reporter::GetClientProductNameAndVersion(&info.product_name,
                                                 &info.version, &info.channel);
#endif
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  GetOsVersion(os_major_version, os_minor_version, os_bugfix_version);
  info.os_version = base::StringPrintf("%d.%d.%d", os_major_version,
                                       os_minor_version, os_bugfix_version);
  return info;
}

void ChromeJsErrorReportProcessor::SendReport(
    const GURL& url,
    const std::string& body,
    base::ScopedClosureRunner callback_runner,
    base::Time report_time,
    network::SharedURLLoaderFactory* loader_factory) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "POST";
  resource_request->url = url;

  const auto traffic_annotation =
      net::DefineNetworkTrafficAnnotation("javascript_report_error", R"(
      semantics {
        sender: "JavaScript error reporter"
        description:
          "Chrome can send JavaScript errors that occur within built-in "
          "component extensions and chrome:// webpages. If enabled, the error "
          "message, along with information about Chrome and the operating "
          "system, is sent to Google for debugging."
        trigger:
          "A JavaScript error occurs in a Chrome component extension (an "
          "extension bundled with the Chrome browser, not downloaded "
          "separately) or in certain chrome:// webpages."
        data:
          "The JavaScript error message, the version and channel of Chrome, "
          "the URL of the extension or webpage, the line and column number of "
          "the JavaScript code where the error occurred, and a stack trace of "
          "the error."
        destination: GOOGLE_OWNED_SERVICE
      }
      policy {
        cookies_allowed: NO
        setting:
          "You can enable or disable this feature via 'Automatically send "
          "usage statistics and crash reports to Google' in Chromium's "
          "settings under Advanced, Privacy. (This is in System Settings on "
          "Chromebooks.) This feature is enabled by default."
        chrome_policy {
          MetricsReportingEnabled {
            policy_options {mode: MANDATORY}
            MetricsReportingEnabled: false
          }
        }
      })");

  VLOG(1) << "Sending crash report: " << resource_request->url;

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  if (!body.empty()) {
    url_loader->AttachStringForUpload(body, "text/plain");
  }

  constexpr int kCrashEndpointResponseMaxSizeInBytes = 1024;
  network::SimpleURLLoader* loader = url_loader.get();
  loader->DownloadToString(
      loader_factory,
      base::BindOnce(&ChromeJsErrorReportProcessor::OnRequestComplete, this,
                     std::move(url_loader), std::move(callback_runner),
                     report_time),
      kCrashEndpointResponseMaxSizeInBytes);
}

// Finishes sending process once the MayBlock processing is done. On UI thread.
void ChromeJsErrorReportProcessor::OnConsentCheckCompleted(
    base::ScopedClosureRunner callback_runner,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    base::TimeDelta browser_process_uptime,
    base::Time report_time,
    base::Optional<JavaScriptErrorReport> error_report) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!error_report) {
    // User didn't consent. This isn't an error so don't log an error.
    return;
  }

  std::string crash_endpoint_string = error_report->send_to_production_servers
                                          ? GetCrashEndpoint()
                                          : GetCrashEndpointStaging();

  // TODO(https://crbug.com/986166): Use crash_reporter for Chrome OS.
  const auto platform = GetPlatformInfo();

  const GURL source(error_report->url);
  const auto product = error_report->product.empty() ? platform.product_name
                                                     : error_report->product;
  const auto version =
      error_report->version.empty() ? platform.version : error_report->version;

  ParameterMap params;
  params["prod"] = net::EscapeQueryParamValue(product, /*use_plus=*/false);
  params["ver"] = net::EscapeQueryParamValue(version, /*use_plus=*/false);
  params["type"] = "JavascriptError";
  params["error_message"] = error_report->message;
  params["browser"] = "Chrome";
  params["browser_version"] = platform.version;
  params["channel"] = platform.channel;
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // base::SysInfo::OperatingSystemName() returns "Linux" on ChromeOS devices.
  params["os"] = "ChromeOS";
#else
  params["os"] = base::SysInfo::OperatingSystemName();
#endif
  params["os_version"] = platform.os_version;
  params["full_url"] = source.spec();
  params["url"] = source.path();
  params["src"] = source.spec();
  if (error_report->line_number)
    params["line"] = base::NumberToString(*error_report->line_number);
  if (error_report->column_number)
    params["column"] = base::NumberToString(*error_report->column_number);
  // TODO(crbug/1121816): Chrome crashes have "Process uptime" and "Process
  // type" fields, eventually consider using that for process uptime.
  params["browser_process_uptime_ms"] =
      base::NumberToString(browser_process_uptime.InMilliseconds());
  params["renderer_process_uptime_ms"] =
      base::NumberToString(error_report->renderer_process_uptime_ms);
  if (error_report->window_type.has_value()) {
    std::string window_type =
        MapWindowTypeToString(error_report->window_type.value());
    if (window_type != kNoBrowserNoWindow)
      params["window_type"] = window_type;
  }
  if (error_report->app_locale)
    params["app_locale"] = std::move(*error_report->app_locale);
  const GURL url(base::StrCat(
      {crash_endpoint_string, "?", BuildPostRequestQueryString(params)}));
  std::string body;
  if (error_report->stack_trace) {
    body = std::move(*error_report->stack_trace);
  }

  SendReport(url, body, std::move(callback_runner), report_time,
             loader_factory.get());
}

void ChromeJsErrorReportProcessor::CheckAndUpdateRecentErrorReports(
    const JavaScriptErrorReport& error_report,
    bool* should_send) {
  base::Time now = clock_->Now();
  constexpr base::TimeDelta kTimeBetweenCleanings =
      base::TimeDelta::FromHours(1);
  constexpr base::TimeDelta kTimeBetweenDuplicateReports =
      base::TimeDelta::FromHours(1);
  // Check for cleaning.
  if (last_recent_error_reports_cleaning_.is_null()) {
    // First time in this function, no need to clean.
    last_recent_error_reports_cleaning_ = now;
  } else if (now - kTimeBetweenCleanings >
             last_recent_error_reports_cleaning_) {
    auto it = recent_error_reports_.begin();
    while (it != recent_error_reports_.end()) {
      if (now - kTimeBetweenDuplicateReports > it->second) {
        it = recent_error_reports_.erase(it);
      } else {
        ++it;
      }
    }
    last_recent_error_reports_cleaning_ = now;
  } else if (now < last_recent_error_reports_cleaning_) {
    // Time went backwards, clock must have been adjusted. Assume all our
    // last-send records are meaningless. Clock adjustments should be rare
    // enough that it doesn't matter if we send a few duplicate reports in this
    // case.
    recent_error_reports_.clear();
    last_recent_error_reports_cleaning_ = now;
  }

  std::string error_key =
      base::StrCat({error_report.message, "+", error_report.product});
  if (error_report.line_number) {
    base::StrAppend(&error_key,
                    {"+", base::NumberToString(*error_report.line_number)});
  }
  if (error_report.column_number) {
    base::StrAppend(&error_key,
                    {"+", base::NumberToString(*error_report.column_number)});
  }
  auto insert_result = recent_error_reports_.try_emplace(error_key, now);
  if (insert_result.second) {
    // No recent reports with this key. Time is already inserted into map.
    *should_send = true;
    return;
  }

  base::Time& last_error_report = insert_result.first->second;
  if (now - kTimeBetweenDuplicateReports > last_error_report ||
      now < last_error_report) {
    // It's been long enough, send the report. (Or, the clock has been adjusted
    // and we don't really know how long it's been, so send the report.)
    *should_send = true;
    last_error_report = now;
  } else {
    *should_send = false;
  }
}

// static
void ChromeJsErrorReportProcessor::Create() {
  // Google only wants error reports from official builds. Don't install a
  // processor for other builds.
#if defined(GOOGLE_CHROME_BUILD)
  DCHECK(JsErrorReportProcessor::Get() == nullptr)
      << "Attempted to create multiple ChromeJsErrorReportProcessors";
  VLOG(3) << "Installing ChromeJsErrorReportProcessor as JavaScript error "
             "processor";
  JsErrorReportProcessor::SetDefault(
      base::AdoptRef(new ChromeJsErrorReportProcessor));
#else
  VLOG(3) << "Not installing ChromeJsErrorReportProcessor as JavaScript error "
          << "processor; not a Google Chrome build";
#endif  // defined(GOOGLE_CHROME_BUILD)
}

void ChromeJsErrorReportProcessor::SendErrorReport(
    JavaScriptErrorReport error_report,
    base::OnceClosure completion_callback,
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // In theory, we should wait until the after the consent check to update the
  // sent map. However, that would mean we do a bunch of extra work on each
  // duplicate; problematic if we're getting a lot of duplicates. In practice,
  // it doesn't matter much if we update the sent map and then find out the user
  // didn't consent -- we will suppress all reports from this instance anyways,
  // so whether we suppress the next report because it was a duplicate or
  // because it failed the consent check doesn't matter.
  bool should_send = false;
  CheckAndUpdateRecentErrorReports(error_report, &should_send);
  if (!should_send) {
    VLOG(3) << "Not sending duplicate error report";
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, std::move(completion_callback));
    return;
  }

  base::ScopedClosureRunner callback_runner(std::move(completion_callback));

  // loader_factory must be created on UI thread. Get it now while we still
  // know the browser_context pointer is valid.
  scoped_refptr<network::SharedURLLoaderFactory> loader_factory =
      content::BrowserContext::GetDefaultStoragePartition(browser_context)
          ->GetURLLoaderFactoryForBrowserProcess();

  // Get browser uptime before swapping threads to reduce lag time between the
  // error report occurring and sending it off.
  base::TimeTicks startup_time = startup_metric_utils::MainEntryPointTicks();
  base::TimeDelta browser_process_uptime =
      (base::TimeTicks::Now() - startup_time);

  // Consent check needs to be done on a blockable thread. We must return to
  // this thread (the UI thread) to use the loader_factory.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&ChromeJsErrorReportProcessor::CheckConsentAndRedact, this,
                     std::move(error_report)),
      base::BindOnce(&ChromeJsErrorReportProcessor::OnConsentCheckCompleted,
                     this, std::move(callback_runner),
                     std::move(loader_factory), browser_process_uptime,
                     clock_->Now()));
}

std::string ChromeJsErrorReportProcessor::GetCrashEndpoint() {
  return kCrashEndpointUrl;
}

std::string ChromeJsErrorReportProcessor::GetCrashEndpointStaging() {
  return kCrashEndpointStagingUrl;
}

void ChromeJsErrorReportProcessor::GetOsVersion(int32_t& os_major_version,
                                                int32_t& os_minor_version,
                                                int32_t& os_bugfix_version) {
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);
}
