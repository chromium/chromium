// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/error_reporting/chrome_js_error_report_processor.h"

#include <stddef.h>

#include <string_view>
#include <tuple>
#include <utility>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"
#include "components/crash/core/app/client_upload_info.h"
#include "components/crash/core/app/crashpad.h"
#include "components/feedback/redaction_tool/redaction_tool.h"
#include "components/startup_metric_utils/common/startup_metric_utils.h"
#include "components/variations/variations_crash_keys.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/build_time.h"
#endif

namespace {

constexpr char kNoBrowserNoWindow[] = "NO_BROWSER";
constexpr char kRegularTabbedWindow[] = "REGULAR_TABBED";
constexpr char kWebAppWindow[] = "WEB_APP";
constexpr char kSystemWebAppWindow[] = "SYSTEM_WEB_APP";

#if BUILDFLAG(IS_CHROMEOS)
// Give up if crash_reporter hasn't finished in this long.
constexpr base::TimeDelta kMaximumWaitForCrashReporter = base::Minutes(1);
#endif

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
  return redaction::RedactionTool(/*first_party_extension_ids=*/nullptr)
      .Redact(message);
}

// Truncate the error message to no more than 1000 characters. Long messages
// are not useful and can cause problems in internal systems (such as
// excessively long URLs used to point to error reports). Note that the
// truncation is calculated pre-character-escaping ("  " is 3 characters, not
// the 9 of "%20%20%20") so that we don't break an escape sequence.
//
// Return the original message if it's already less than 1000 characters, or
// a truncated version if it's over 1000 characters
std::string TruncateErrorMessage(const std::string& message) {
  constexpr int kMaxCharacters = 1000;

  if (message.length() <= kMaxCharacters) {
    return message;
  }

  constexpr std::string_view kTruncationMessage = "--[TRUNCATED]--";
  constexpr int kTruncationMessageLength = kTruncationMessage.size();

  // Truncate the middle of the message. The useful information is likely to be
  // at the beginning ('Invalid regex: "....."') or the end ('"...." is not
  // a valid email address').
  constexpr int kStartLength =
      (kMaxCharacters - kTruncationMessageLength + 1) / 2;
  constexpr int kEndLength = (kMaxCharacters - kTruncationMessageLength) / 2;
  std::string::size_type begin_end_fragment = message.length() - kEndLength;

  return base::StrCat({message.substr(0, kStartLength), kTruncationMessage,
                       message.substr(begin_end_fragment)});
}

std::string MapWindowTypeToString(
    JavaScriptErrorReport::WindowType window_type) {
  switch (window_type) {
    case JavaScriptErrorReport::WindowType::kRegularTabbed:
      return kRegularTabbedWindow;
    case JavaScriptErrorReport::WindowType::kWebApp:
      return kWebAppWindow;
    case JavaScriptErrorReport::WindowType::kSystemWebApp:
      return kSystemWebAppWindow;
    default:
      return kNoBrowserNoWindow;
  }
}

}  // namespace

ChromeJsErrorReportProcessor::ChromeJsErrorReportProcessor()
    :
#if BUILDFLAG(IS_CHROMEOS)
      maximium_wait_for_crash_reporter_(kMaximumWaitForCrashReporter),
#endif
      clock_(base::DefaultClock::GetInstance()) {
}
ChromeJsErrorReportProcessor::~ChromeJsErrorReportProcessor() = default;

// Returns the redacted, fixed-up error report if the user consented to have it
// sent. Returns std::nullopt if the user did not consent or we otherwise
// should not send the report. All the MayBlock work should be done in here.
std::optional<JavaScriptErrorReport>
ChromeJsErrorReportProcessor::CheckConsentAndRedact(
    JavaScriptErrorReport error_report) {
  // Consent is handled at the OS level by crash_reporter so we don't need to
  // check it here for Chrome OS.
#if !BUILDFLAG(IS_CHROMEOS)
  if (!crash_reporter::GetClientCollectStatsConsent()) {
    return std::nullopt;
  }
#endif

  // Remove error message from stack trace before redaction, since redaction
  // might change the error message enough that we don't find it.
  if (error_report.stack_trace) {
    RemoveErrorMessageFromStackTrace(error_report.message,
                                     *error_report.stack_trace);
  }

  error_report.message = RedactErrorMessage(error_report.message);
  // TODO(crbug.com/40146362): Also redact stack trace, but don't
  // completely remove the URL (only query & fragment).
  return error_report;
}

struct ChromeJsErrorReportProcessor::PlatformInfo {
  std::string product_name;
  std::string version;
  std::string channel;
};

ChromeJsErrorReportProcessor::PlatformInfo
ChromeJsErrorReportProcessor::GetPlatformInfo() {
  PlatformInfo info;

  // TODO(crbug.com/40146362): Get correct product_name for non-POSIX
  // platforms.
#if BUILDFLAG(IS_POSIX)
  crash_reporter::GetClientProductNameAndVersion(&info.product_name,
                                                 &info.version, &info.channel);
#endif
  return info;
}

variations::ExperimentListInfo
ChromeJsErrorReportProcessor::GetExperimentListInfo() const {
  return variations::GetExperimentListInfo();
}

void ChromeJsErrorReportProcessor::AddExperimentIds(ParameterMap& params) {
  variations::ExperimentListInfo experiment_info = GetExperimentListInfo();

  params[variations::kNumExperimentsKey] =
      base::NumberToString(experiment_info.num_experiments);
  params[variations::kExperimentListKey] = experiment_info.experiment_list;
}

// Finishes sending process once the MayBlock processing is done. On UI thread.
void ChromeJsErrorReportProcessor::OnConsentCheckCompleted(
    base::ScopedClosureRunner callback_runner,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    base::TimeDelta browser_process_uptime,
    base::Time report_time,
    std::optional<JavaScriptErrorReport> error_report) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!error_report) {
    // User didn't consent. This isn't an error so don't log an error.
    return;
  }

  const auto platform = GetPlatformInfo();
  const GURL source(error_report->url);
  const auto product = error_report->product.empty() ? platform.product_name
                                                     : error_report->product;
  const auto version =
      error_report->version.empty() ? platform.version : error_report->version;

  ParameterMap params;
  params["prod"] = base::EscapeQueryParamValue(product, /*use_plus=*/false);
  params["ver"] = base::EscapeQueryParamValue(version, /*use_plus=*/false);
  params["type"] = "JavascriptError";
  params["error_message"] = TruncateErrorMessage(error_report->message);
  params["browser"] = "Chrome";
  params["browser_version"] = platform.version;
  params["channel"] = platform.channel;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  int64_t build_time =
      (base::GetBuildTime() - base::Time::UnixEpoch()).InMilliseconds();
  params["build_time_millis"] = base::NumberToString(build_time);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // base::SysInfo::OperatingSystemName() returns "Linux" on ChromeOS devices.
  params["os"] = "ChromeOS";
#else
  params["os"] = base::SysInfo::OperatingSystemName();
  params["os_version"] = GetOsVersion();
#endif
  constexpr char kSourceSystemParamName[] = "source_system";
  switch (error_report->source_system) {
    case JavaScriptErrorReport::SourceSystem::kUnknown:
      break;
    case JavaScriptErrorReport::SourceSystem::kCrashReportApi:
      params[kSourceSystemParamName] = "crash_report_api";
      break;
    case JavaScriptErrorReport::SourceSystem::kWebUIObserver:
      params[kSourceSystemParamName] = "webui_observer";
      break;
    case JavaScriptErrorReport::SourceSystem::kDevToolsObserver:
      params[kSourceSystemParamName] = "devtools_observer";
      break;
  }
  params["full_url"] = source.spec();
  params["url"] = source.path();
  params["src"] = source.spec();
  if (error_report->line_number)
    params["line"] = base::NumberToString(*error_report->line_number);
  if (error_report->column_number)
    params["column"] = base::NumberToString(*error_report->column_number);
  if (error_report->debug_id)
    params["debug_id"] = std::move(*error_report->debug_id);
  // TODO(crbug.com/40146362): Chrome crashes have "Process uptime" and "Process
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
  if (error_report->page_url)
    params["page_url"] = std::move(*error_report->page_url);
  AddExperimentIds(params);

  SendReport(std::move(params), std::move(error_report->stack_trace),
             error_report->send_to_production_servers,
             std::move(callback_runner), report_time, loader_factory);
}

void ChromeJsErrorReportProcessor::CheckAndUpdateRecentErrorReports(
    const JavaScriptErrorReport& error_report,
    bool* should_send) {
  base::Time now = clock_->Now();
  constexpr base::TimeDelta kTimeBetweenCleanings = base::Hours(1);
  constexpr base::TimeDelta kTimeBetweenDuplicateReports = base::Hours(1);
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
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  DCHECK(JsErrorReportProcessor::Get() == nullptr)
      << "Attempted to create multiple ChromeJsErrorReportProcessors";
  VLOG(3) << "Installing ChromeJsErrorReportProcessor as JavaScript error "
             "processor";
  JsErrorReportProcessor::SetDefault(
      base::AdoptRef(new ChromeJsErrorReportProcessor));
#else
  VLOG(3) << "Not installing ChromeJsErrorReportProcessor as JavaScript error "
          << "processor; not a Google Chrome build";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
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

  scoped_refptr<network::SharedURLLoaderFactory> loader_factory;
#if !BUILDFLAG(IS_CHROMEOS)
  // loader_factory must be created on UI thread. Get it now while we still
  // know the browser_context pointer is valid.
  loader_factory = browser_context->GetDefaultStoragePartition()
                       ->GetURLLoaderFactoryForBrowserProcess();
#endif

  // Get browser uptime before swapping threads to reduce lag time between the
  // error report occurring and sending it off.
  base::TimeTicks startup_time =
      startup_metric_utils::GetCommon().MainEntryPointTicks();
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
