// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/error_reporting/chrome_js_error_report_processor.h"

#include <tuple>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/crash/content/browser/error_reporting/javascript_error_report.h"
#include "components/crash/core/app/client_upload_info.h"
#include "components/feedback/redaction_tool.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/escape.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace {

constexpr char kCrashEndpointUrl[] = "https://clients2.google.com/cr/report";

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

}  // namespace

ChromeJsErrorReportProcessor::ChromeJsErrorReportProcessor() = default;
ChromeJsErrorReportProcessor::~ChromeJsErrorReportProcessor() = default;

void ChromeJsErrorReportProcessor::OnRequestComplete(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    base::ScopedClosureRunner callback_runner,
    std::unique_ptr<std::string> response_body) {
  if (response_body) {
    // TODO(iby): Update the crash log (uploads.log)
    VLOG(1) << "Uploaded crash report. ID: " << *response_body;
  } else {
    LOG(ERROR) << "Failed to upload crash report";
  }
  // callback_runner will implicitly run the callback when we reach this line.
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
          "component extensions. If enabled, the error message, along "
          "with information about Chrome and the operating system, is sent to "
          "Google."
        trigger:
          "A JavaScript error occurs in a Chrome component extension (an "
          "extension bundled with the Chrome browser, not downloaded "
          "separately)."
        data:
          "The JavaScript error message, the version and channel of Chrome, "
          "the URL of the extension, the line and column number where the "
          "error occurred, and a stack trace of the error."
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
                     std::move(url_loader), std::move(callback_runner)),
      kCrashEndpointResponseMaxSizeInBytes);
}

// Finishes sending process once the MayBlock processing is done. On UI thread.
void ChromeJsErrorReportProcessor::OnConsentCheckCompleted(
    base::ScopedClosureRunner callback_runner,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    base::TimeDelta browser_process_uptime,
    base::Optional<JavaScriptErrorReport> error_report) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!error_report) {
    // User didn't consent. This isn't an error so don't log an error.
    return;
  }

  std::string crash_endpoint_string = GetCrashEndpoint();
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
#if defined(OS_CHROMEOS) || BUILDFLAG(IS_LACROS)
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
  if (error_report->app_locale)
    params["app_locale"] = std::move(*error_report->app_locale);
  const GURL url(base::StrCat(
      {crash_endpoint_string, "?", BuildPostRequestQueryString(params)}));
  std::string body;
  if (error_report->stack_trace) {
    body = std::move(*error_report->stack_trace);
  }

  SendReport(url, body, std::move(callback_runner), loader_factory.get());
}

// static
void ChromeJsErrorReportProcessor::Create() {
  // Google only wants error reports from official builds. Don't install a
  // processor for other builds.
#if defined(GOOGLE_CHROME_BUILD)
  DCHECK(JsErrorReportProcessor::Get() == nullptr)
      << "Attempted to create multiple ChromeJsErrorReportProcessors";
  JsErrorReportProcessor::SetDefault(
      base::AdoptRef(new ChromeJsErrorReportProcessor));
#endif  // defined(GOOGLE_CHROME_BUILD)
}

void ChromeJsErrorReportProcessor::SendErrorReport(
    JavaScriptErrorReport error_report,
    base::OnceClosure completion_callback,
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
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
                     std::move(loader_factory), browser_process_uptime));
}

std::string ChromeJsErrorReportProcessor::GetCrashEndpoint() {
  return kCrashEndpointUrl;
}

void ChromeJsErrorReportProcessor::GetOsVersion(int32_t& os_major_version,
                                                int32_t& os_minor_version,
                                                int32_t& os_bugfix_version) {
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);
}
