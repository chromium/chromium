// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/extension_telemetry/extension_telemetry_uploader.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/safe_browsing/core/browser/sync/safe_browsing_primary_account_token_fetcher.h"
#include "components/safe_browsing/core/common/utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace safe_browsing {

namespace {

constexpr const char kUploadUrl[] =
    "https://safebrowsing.google.com/safebrowsing/clientreport/crx-telemetry";

constexpr net::NetworkTrafficAnnotationTag
    kSafeBrowsingExtensionTelemetryTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("safe_browsing_extension_telemetry",
                                            R"(
    semantics {
      sender: "Safe Browsing Extension Telemetry"
      description:
        "Collects information about installed browser extensions and extension "
        "related events (e.g., API invocations). Sends this information to "
        "Google to help detect malware extensions."
      trigger:
        "Periodic upload of telemetry data once every few hours."
      data:
        "A list of currently installed extensions (id, name, version) along "
        "with a list of extension event data associated with each extension."
      destination: GOOGLE_OWNED_SERVICE
    }
    policy {
      cookies_allowed: YES
      cookies_store: "Safe Browsing cookie store"
      setting:
        "Users can enable this feature by selecting 'Enhanced protection' "
        "under the Security->Safe Browsing setting. The feature is disabled by "
        "default."
      chrome_policy {
        SafeBrowsingProtectionLevel {
          SafeBrowsingProtectionLevel: 0
        }
      }
    }
    comments:
      "SafeBrowsingProtectionLevel value of 0 or 1 disables the extension "
      "telemetry feature. A value of 2 enables the feature. The feature is "
      "disabled by default."
    )");
// Constants associated with exponential backoff. On each failure, we will
// increase the backoff by |kBackoffFactor|, starting from
// |kInitialBackoffSeconds|. If we fail after |kMaxRetryAttempts| retries, the
// upload fails.
const int kInitialBackoffSeconds = 1;
const int kBackoffFactor = 2;
const int kMaxRetryAttempts = 2;

void RecordUploadSize(size_t size) {
  base::UmaHistogramCounts1M("SafeBrowsing.ExtensionTelemetry.UploadSize",
                             size);
}

void RecordUploadSuccess(bool success) {
  base::UmaHistogramBoolean("SafeBrowsing.ExtensionTelemetry.UploadSuccess",
                            success);
}

void RecordUploadRetries(int num_retries) {
  base::UmaHistogramExactLinear(
      "SafeBrowsing.ExtensionTelemetry.RetriesTillUploadSuccess", num_retries,
      kMaxRetryAttempts);
}

void RecordNetworkResponseCodeOrError(int code_or_error) {
  base::UmaHistogramSparse(
      "SafeBrowsing.ExtensionTelemetry.NetworkRequestResponseCodeOrError",
      code_or_error);
}

void RecordUploadDuration(bool success, base::TimeDelta delta) {
  if (success) {
    base::UmaHistogramMediumTimes(
        "SafeBrowsing.ExtensionTelemetry.SuccessfulUploadDuration", delta);
  } else {
    base::UmaHistogramMediumTimes(
        "SafeBrowsing.ExtensionTelemetry.FailedUploadDuration", delta);
  }
}

}  // namespace

ExtensionTelemetryUploader::~ExtensionTelemetryUploader() = default;

ExtensionTelemetryUploader::ExtensionTelemetryUploader(
    OnUploadCallback callback,
    const scoped_refptr<network::SharedURLLoaderFactory>& url_loader_factory,
    std::unique_ptr<std::string> upload_data,
    std::unique_ptr<SafeBrowsingTokenFetcher> token_fetcher)
    : callback_(std::move(callback)),
      url_loader_factory_(url_loader_factory),
      upload_data_(std::move(upload_data)),
      current_backoff_(base::Seconds(kInitialBackoffSeconds)),
      num_upload_retries_(0),
      token_fetcher_(std::move(token_fetcher)) {}

void ExtensionTelemetryUploader::Start() {
  upload_start_time_ = base::TimeTicks::Now();
  RecordUploadSize(upload_data_->size());
  MaybeSendRequestWithAccessToken();
}

// static
std::string ExtensionTelemetryUploader::GetUploadURLForTest() {
  return kUploadUrl;
}

void ExtensionTelemetryUploader::MaybeSendRequestWithAccessToken() {
  if (token_fetcher_) {
    token_fetcher_->Start(base::BindOnce(
        &ExtensionTelemetryUploader::SendRequest, weak_factory_.GetWeakPtr()));
  } else {
    SendRequest(std::string());
  }
}

void ExtensionTelemetryUploader::SendRequest(const std::string& access_token) {
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kUploadUrl);
  resource_request->method = "POST";
  resource_request->load_flags = net::LOAD_DISABLE_CACHE;
  if (!access_token.empty()) {
    LogAuthenticatedCookieResets(
        *resource_request,
        SafeBrowsingAuthenticatedEndpoint::kExtensionTelemetry);
    SetAccessTokenAndClearCookieInResourceRequest(resource_request.get(),
                                                  access_token);
  } else {
    resource_request->credentials_mode =
        network::mojom::CredentialsMode::kInclude;
  }
  url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request),
      kSafeBrowsingExtensionTelemetryTrafficAnnotation);
  url_loader_->SetAllowHttpErrorResults(true);
  url_loader_->AttachStringForUpload(*upload_data_, "application/octet-stream");
  url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&ExtensionTelemetryUploader::OnURLLoaderComplete,
                     weak_factory_.GetWeakPtr()));
}

void ExtensionTelemetryUploader::OnURLLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = 0;
  if (url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers)
    response_code = url_loader_->ResponseInfo()->headers->response_code();

  RetryOrFinish(url_loader_->NetError(), response_code, *response_body.get());
}

void ExtensionTelemetryUploader::RetryOrFinish(
    int net_error,
    int response_code,
    const std::string& response_data) {
  RecordNetworkResponseCodeOrError(net_error == net::OK ? response_code
                                                        : net_error);
  if (net_error == net::OK && response_code == net::HTTP_OK) {
    RecordUploadSuccess(/*success*/ true);
    RecordUploadRetries(num_upload_retries_);
    RecordUploadDuration(/*success*/ true,
                         base::TimeTicks::Now() - upload_start_time_);
    // Callback may delete the uploader, so no touching anything after this.
    std::move(callback_).Run(/*success=*/true, response_data);
  } else {
    if (response_code < 500 || num_upload_retries_ >= kMaxRetryAttempts) {
      RecordUploadSuccess(/*success*/ false);
      RecordUploadDuration(/*success*/ false,
                           base::TimeTicks::Now() - upload_start_time_);
      // Callback may delete the uploader, so no touching anything after this.
      std::move(callback_).Run(/*success=*/false, response_data);
    } else {
      content::GetUIThreadTaskRunner({})->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              &ExtensionTelemetryUploader::MaybeSendRequestWithAccessToken,
              weak_factory_.GetWeakPtr()),
          current_backoff_);
      current_backoff_ *= kBackoffFactor;
      num_upload_retries_++;
    }
  }
}

}  // namespace safe_browsing
