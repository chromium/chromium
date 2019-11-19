// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/profile_resetter/profile_reset_report.pb.h"
#include "chrome/browser/profile_resetter/reset_report_uploader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"

namespace {
const char kResetReportUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/chrome-reset";

GURL GetClientReportUrl(const std::string& report_url) {
  GURL url(report_url);
  std::string api_key = google_apis::GetAPIKey();
  if (!api_key.empty())
    url = url.Resolve("?key=" + net::EscapeQueryParamValue(api_key, true));

  return url;
}

}  // namespace

ResetReportUploader::ResetReportUploader(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

ResetReportUploader::~ResetReportUploader() {}

void ResetReportUploader::DispatchReport(
    const reset_report::ChromeResetReport& report) {
  std::string request_data;
  CHECK(report.SerializeToString(&request_data));

  DispatchReportInternal(request_data);
}

void ResetReportUploader::DispatchReportInternal(
    const std::string& request_data) {
  // Create traffic annotation tag.
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("profile_resetter_upload", R"(
        semantics {
          sender: "Profile Resetter"
          description:
            "When users choose to reset their profile, they are offered the "
            "choice to report to Google the settings and their values that are "
            "affected by the reset. The user can inspect the values before "
            "they are sent to Google and needs to consent to sending them."
          trigger:
            "Users reset their profile in Chrome settings and consent to "
            "sending a report."
          data:
            "Startup URLs, homepage URL, default search engine, installed "
            "extensions, Chrome shortcut on the desktop and the Windows start "
            "menu, some settings. See "
            "chrome/browser/profile_resetter/profile_reset_report.proto "
            "for details."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting: "None, the user needs to actively send the data."
          policy_exception_justification:
            "None, considered not useful because the user needs to actively "
            "send the data."
        })");

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetClientReportUrl(kResetReportUrl);
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->method = "POST";
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader =
      network::SimpleURLLoader::Create(std::move(resource_request),
                                       traffic_annotation);
  simple_url_loader->AttachStringForUpload(request_data,
                                           "application/octet-stream");
  auto it = simple_url_loaders_.insert(simple_url_loaders_.begin(),
                                       std::move(simple_url_loader));
  it->get()->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ResetReportUploader::OnSimpleLoaderComplete,
                     base::Unretained(this), std::move(it)));
}

void ResetReportUploader::OnSimpleLoaderComplete(
    SimpleURLLoaderList::iterator it,
    std::unique_ptr<std::string> response_body) {
  simple_url_loaders_.erase(it);
}

GURL ResetReportUploader::GetClientReportUrlForTesting() {
  return GetClientReportUrl(kResetReportUrl);
}
