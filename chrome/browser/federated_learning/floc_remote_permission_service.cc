// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/federated_learning/floc_remote_permission_service.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace federated_learning {

namespace {

const char kQueryFlocPermissionUrl[] =
    "https://adservice.google.com/settings/do_ad_settings_allow_floc_poc";

// The maximum number of retries for the SimpleURLLoader requests.
const size_t kMaxRetries = 1;

class RequestImpl : public FlocRemotePermissionService::Request {
 public:
  RequestImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      const GURL& url,
      FlocRemotePermissionService::CreateRequestCallback callback,
      const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation)
      : url_loader_factory_(std::move(url_loader_factory)),
        url_(url),
        response_code_(0),
        callback_(std::move(callback)),
        partial_traffic_annotation_(partial_traffic_annotation) {
    DCHECK(url_loader_factory_);
  }

  ~RequestImpl() override = default;

  // Returns the response code received from the server, which will only be
  // valid if the request succeeded.
  int GetResponseCode() override { return response_code_; }

  // Returns the contents of the response body received from the server.
  const std::string& GetResponseBody() override { return response_body_; }

 private:
  void Start() override {
    auto resource_request = std::make_unique<network::ResourceRequest>();
    resource_request->url = url_;
    resource_request->site_for_cookies = net::SiteForCookies::FromUrl(url_);
    resource_request->trusted_params =
        network::ResourceRequest::TrustedParams();
    resource_request->trusted_params->isolation_info =
        net::IsolationInfo::CreateForInternalRequest(url::Origin::Create(url_));
    resource_request->method = "GET";

    DCHECK(resource_request->SendsCookies());

    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::CompleteNetworkTrafficAnnotation("floc_remote_permission_service",
                                              partial_traffic_annotation_,
                                              R"(
          semantics {
            sender: "Federated Learning of Cohorts Remote Permission Service"
            destination: GOOGLE_OWNED_SERVICE
          }
          policy {
            cookies_allowed: YES
            cookies_store: "user"
            chrome_policy {
              SyncDisabled {
                SyncDisabled: true
              }
            }
          })");

    simple_url_loader_ = network::SimpleURLLoader::Create(
        std::move(resource_request), traffic_annotation);
    simple_url_loader_->SetRetryOptions(kMaxRetries,
                                        network::SimpleURLLoader::RETRY_ON_5XX);
    simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        url_loader_factory_.get(),
        base::BindOnce(&RequestImpl::OnSimpleLoaderComplete,
                       base::Unretained(this)));
  }

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body) {
    response_code_ = -1;
    if (simple_url_loader_->ResponseInfo() &&
        simple_url_loader_->ResponseInfo()->headers) {
      response_code_ =
          simple_url_loader_->ResponseInfo()->headers->response_code();
    }
    simple_url_loader_.reset();

    if (response_body) {
      response_body_ = std::move(*response_body);
    } else {
      response_body_.clear();
    }
    std::move(callback_).Run(this);
    // It is valid for the callback to delete |this|, so do not access any
    // members below here.
  }

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The URL of the API endpoint.
  const GURL url_;

  // Handles the actual request to |url_| (the API endpoint).
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Holds the response code received from the server.
  int response_code_;

  // Holds the response body received from the server.
  std::string response_body_;

  // The callback to execute when the query is complete.
  FlocRemotePermissionService::CreateRequestCallback callback_;

  // Partial Network traffic annotation used to create SimpleURLLoader for this
  // request.
  const net::PartialNetworkTrafficAnnotationTag partial_traffic_annotation_;
};

std::unique_ptr<base::ListValue> ReadResponseAsList(
    FlocRemotePermissionService::Request* request) {
  std::unique_ptr<base::ListValue> result;
  if (request->GetResponseCode() == net::HTTP_OK) {
    std::unique_ptr<base::Value> value =
        base::JSONReader::ReadDeprecated(request->GetResponseBody());
    if (value && value->is_list())
      result.reset(static_cast<base::ListValue*>(value.release()));
    else
      DLOG(WARNING) << "Non-JSON-Array response received from the server.";
  }
  return result;
}

}  // namespace

FlocRemotePermissionService::Request::Request() = default;

FlocRemotePermissionService::Request::~Request() = default;

FlocRemotePermissionService::FlocRemotePermissionService(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(std::move(url_loader_factory)) {}

FlocRemotePermissionService::~FlocRemotePermissionService() = default;

std::unique_ptr<FlocRemotePermissionService::Request>
FlocRemotePermissionService::CreateRequest(
    const GURL& url,
    CreateRequestCallback callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  return std::make_unique<RequestImpl>(url_loader_factory_, url,
                                       std::move(callback),
                                       partial_traffic_annotation);
}

GURL FlocRemotePermissionService::GetQueryFlocPermissionUrl() const {
  return GURL(kQueryFlocPermissionUrl);
}

void FlocRemotePermissionService::QueryFlocPermission(
    QueryFlocPermissionCallback callback,
    const net::PartialNetworkTrafficAnnotationTag& partial_traffic_annotation) {
  // Wrap the original callback into a generic CreateRequestCallback.
  CreateRequestCallback create_request_callback = base::BindOnce(
      &FlocRemotePermissionService::QueryFlocPermissionCompletionCallback,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback));

  GURL url = GetQueryFlocPermissionUrl();
  std::unique_ptr<Request> request = CreateRequest(
      url, std::move(create_request_callback), partial_traffic_annotation);
  Request* request_raw_ptr = request.get();
  pending_floc_permission_requests_[request_raw_ptr] = std::move(request);
  request_raw_ptr->Start();
}

void FlocRemotePermissionService::QueryFlocPermissionCompletionCallback(
    FlocRemotePermissionService::QueryFlocPermissionCallback callback,
    FlocRemotePermissionService::Request* request) {
  std::unique_ptr<Request> request_ptr =
      std::move(pending_floc_permission_requests_[request]);
  pending_floc_permission_requests_.erase(request);

  std::unique_ptr<base::ListValue> response_value;
  bool swaa = false;
  bool nac = false;
  bool account_type = false;

  response_value = ReadResponseAsList(request);
  if (response_value) {
    base::Value::ListView l = response_value->GetList();
    if (l.size() == 3) {
      if (l[0].is_bool())
        swaa = l[0].GetBool();
      if (l[1].is_bool())
        nac = l[1].GetBool();
      if (l[2].is_bool())
        account_type = l[2].GetBool();
    }
  }

  std::move(callback).Run(swaa && nac && account_type);
}

}  // namespace federated_learning
