// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/incident_report_uploader_impl.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/escape.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "google_apis/google_api_keys.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace safe_browsing {

namespace {

const char kSbIncidentReportUrl[] =
    "https://sb-ssl.google.com/safebrowsing/clientreport/incident";

constexpr net::NetworkTrafficAnnotationTag
    kSafeBrowsingIncidentTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("safe_browsing_incident", R"(
    semantics {
      sender: "Safe Browsing Incident Reporting"
      description:
        "Following a security incident, Chrome can report system information "
        "and possible causes to Google to improve Safe Browsing experience."
      trigger:
        "An incident on the local machine affecting the user's experience with "
        "Chrome."
      data:
        "A description of the incident, possible causes and related system "
        "information. See ClientIncidentReport in 'https://cs.chromium.org/"
        "chromium/src/components/safe_browsing/csd.proto' for more details."
      destination: GOOGLE_OWNED_SERVICE
    }
    policy {
      cookies_allowed: YES
      cookies_store: "Safe Browsing cookie store"
      setting:
        "Users can control this feature via the 'Automatically report details "
        "of possible security incidents to Google' setting under Privacy."
      chrome_policy {
        SafeBrowsingExtendedReportingEnabled {
          policy_options {mode: MANDATORY}
          SafeBrowsingExtendedReportingEnabled: false
        }
      }
    })");

}  // namespace

// This is initialized here rather than in the class definition due to an
// "extension" in MSVC that defies the standard.
// static
const int IncidentReportUploaderImpl::kTestUrlFetcherId = 47;

IncidentReportUploaderImpl::~IncidentReportUploaderImpl() {
}

// static
std::unique_ptr<IncidentReportUploader>
IncidentReportUploaderImpl::UploadReport(
    OnResultCallback callback,
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    const ClientIncidentReport& report) {
  std::string post_data;
  if (!report.SerializeToString(&post_data))
    return nullptr;
  return std::unique_ptr<IncidentReportUploader>(new IncidentReportUploaderImpl(
      std::move(callback), url_loader_factory, post_data));
}

IncidentReportUploaderImpl::IncidentReportUploaderImpl(
    OnResultCallback callback,
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    const std::string& post_data)
    : IncidentReportUploader(std::move(callback)) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GetIncidentReportUrl();
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request), kSafeBrowsingIncidentTrafficAnnotation);
  url_loader_->AttachStringForUpload(post_data, "application/octet-stream");
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory.get(),
      base::BindOnce(&IncidentReportUploaderImpl::OnURLLoaderComplete,
                     base::Unretained(this)));
  time_begin_ = base::TimeTicks::Now();
  UMA_HISTOGRAM_COUNTS_1M("SBIRS.ReportPayloadSize", post_data.size());
}

// static
GURL IncidentReportUploaderImpl::GetIncidentReportUrl() {
  GURL url(kSbIncidentReportUrl);
  std::string api_key(google_apis::GetAPIKey());
  if (api_key.empty())
    return url;
  return url.Resolve("?key=" + base::EscapeQueryParamValue(api_key, true));
}

void IncidentReportUploaderImpl::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  // Take ownership of the loader in this scope.
  std::unique_ptr<network::SimpleURLLoader> url_loader(std::move(url_loader_));
  int response_code = 0;
  if (url_loader->ResponseInfo() && url_loader->ResponseInfo()->headers)
    response_code = url_loader->ResponseInfo()->headers->response_code();

  std::string response_body_str;
  if (response_body.get())
    response_body_str = std::move(*response_body.get());

  OnURLLoaderCompleteInternal(response_body_str, response_code,
                              url_loader->NetError());
}

void IncidentReportUploaderImpl::OnURLLoaderCompleteInternal(
    const std::string& response_body,
    int response_code,
    int net_error) {
  Result result = UPLOAD_REQUEST_FAILED;
  std::unique_ptr<ClientIncidentResponse> response;
  if (net_error == net::OK && response_code == net::HTTP_OK) {
    response = std::make_unique<ClientIncidentResponse>();
    if (!response->ParseFromString(response_body)) {
      response.reset();
      result = UPLOAD_INVALID_RESPONSE;
    } else {
      result = UPLOAD_SUCCESS;
    }
  }
  // Callbacks have a tendency to delete the uploader, so no touching anything
  // after this.
  std::move(callback_).Run(result, std::move(response));
}

}  // namespace safe_browsing
