// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/error_reporting/chrome_js_error_report_processor.h"

#include <utility>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/common/chrome_paths.h"
#include "components/upload_list/crash_upload_list.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace {

constexpr char kCrashEndpointUrl[] = "https://clients2.google.com/cr/report";
constexpr char kCrashEndpointStagingUrl[] =
    "https://clients2.google.com/cr/staging_report";

}  // namespace

void ChromeJsErrorReportProcessor::OnRequestComplete(
    std::unique_ptr<network::SimpleURLLoader> url_loader,
    base::ScopedClosureRunner callback_runner,
    base::Time report_time,
    std::unique_ptr<std::string> response_body) {
  if (response_body) {
    DVLOG(1) << "Uploaded crash report. ID: " << *response_body;
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&ChromeJsErrorReportProcessor::UpdateReportDatabase,
                       this, std::move(*response_body), report_time),
        callback_runner.Release());
  } else {
    DLOG(ERROR) << "Failed to upload crash report";
  }
  // callback_runner may implicitly run the callback when we reach this line if
  // we didn't add a task to update the report database.
}

std::string ChromeJsErrorReportProcessor::BuildPostRequestQueryString(
    const ParameterMap& params) {
  std::vector<std::string> query_parts;
  for (const auto& kv : params) {
    query_parts.push_back(base::StrCat(
        {kv.first, "=",
         base::EscapeQueryParamValue(kv.second, /*use_plus=*/false)}));
  }
  return base::JoinString(query_parts, "&");
}

void ChromeJsErrorReportProcessor::UpdateReportDatabase(
    std::string remote_report_id,
    base::Time report_time) {
  // Uploads.log format is "seconds_since_epoch,crash_id\n"
  base::FilePath crash_dir_path;
  if (!base::PathService::Get(chrome::DIR_CRASH_DUMPS, &crash_dir_path)) {
    DVLOG(1) << "Nowhere to write uploads.log";
    return;
  }
  base::FilePath upload_log_path =
      crash_dir_path.AppendASCII(CrashUploadList::kReporterLogFilename);
  base::File upload_log(upload_log_path,
                        base::File::FLAG_OPEN_ALWAYS | base::File::FLAG_APPEND);
  if (!upload_log.IsValid()) {
    DVLOG(1) << "Could not open upload.log: "
             << base::File::ErrorToString(upload_log.error_details());
    return;
  }
  std::string line = base::StrCat({base::NumberToString(report_time.ToTimeT()),
                                   ",", remote_report_id, "\n"});
  // WriteAtCurrentPos because O_APPEND.
  if (!upload_log.WriteAtCurrentPosAndCheck(base::as_byte_span(line))) {
    DVLOG(1) << "Could not write to upload.log";
    return;
  }
}

std::string ChromeJsErrorReportProcessor::GetCrashEndpoint() {
  return kCrashEndpointUrl;
}

std::string ChromeJsErrorReportProcessor::GetCrashEndpointStaging() {
  return kCrashEndpointStagingUrl;
}

// On non-Chrome OS platforms, send the report directly.
void ChromeJsErrorReportProcessor::SendReport(
    ParameterMap params,
    std::optional<std::string> stack_trace,
    bool send_to_production_servers,
    base::ScopedClosureRunner callback_runner,
    base::Time report_time,
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory) {
  std::string crash_endpoint_string = send_to_production_servers
                                          ? GetCrashEndpoint()
                                          : GetCrashEndpointStaging();

  const GURL url(base::StrCat(
      {crash_endpoint_string, "?", BuildPostRequestQueryString(params)}));
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->method = "POST";
  resource_request->url = url;

  const auto traffic_annotation =
      net::DefineNetworkTrafficAnnotation("javascript_report_error", R"(
      semantics {
        sender: "JavaScript error reporter"
        description:
          "Chrome can send JavaScript errors that occur within built-in "
          "component extensions, chrome:// webpages and DevTools. If enabled, "
          "the error message, along with information about Chrome and the "
          "operating system, is sent to Google for debugging."
        trigger:
          "A JavaScript error occurs in a Chrome component extension (an "
          "extension bundled with the Chrome browser, not downloaded "
          "separately) or in certain chrome:// webpages or "
          "in Chrome DevTools (devtools:// pages)."
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

  DVLOG(1) << "Sending crash report: " << resource_request->url;

  auto url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), traffic_annotation);

  if (stack_trace) {
    url_loader->AttachStringForUpload(*stack_trace, "text/plain");
  }

  constexpr int kCrashEndpointResponseMaxSizeInBytes = 1024;
  network::SimpleURLLoader* loader = url_loader.get();
  loader->DownloadToString(
      loader_factory.get(),
      base::BindOnce(&ChromeJsErrorReportProcessor::OnRequestComplete, this,
                     std::move(url_loader), std::move(callback_runner),
                     report_time),
      kCrashEndpointResponseMaxSizeInBytes);
}

std::string ChromeJsErrorReportProcessor::GetOsVersion() {
  int32_t os_major_version = 0;
  int32_t os_minor_version = 0;
  int32_t os_bugfix_version = 0;
  base::SysInfo::OperatingSystemVersionNumbers(
      &os_major_version, &os_minor_version, &os_bugfix_version);
  return base::StrCat({base::NumberToString(os_major_version), ".",
                       base::NumberToString(os_minor_version), ".",
                       base::NumberToString(os_bugfix_version)});
}
