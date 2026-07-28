// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/remote_actor/remote_actor_request_helper.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/oauth_consumer_id.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "google_apis/gaia/gaia_urls.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/load_flags.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/gurl.h"

namespace password_manager {

namespace {
constexpr int kMaxRetries = 1;
}  // namespace

RemoteActorRequest::RemoteActorRequest(
    signin::IdentityManager* identity_manager,
    const GURL& url,
    const std::string& method,
    const std::string& post_data,
    signin::OAuthConsumerId consumer_id,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    CompletionCallback completion_callback)
    : identity_manager_(identity_manager),
      url_(url),
      method_(method),
      post_data_(post_data),
      consumer_id_(consumer_id),
      traffic_annotation_(traffic_annotation),
      url_loader_factory_(std::move(url_loader_factory)),
      completion_callback_(std::move(completion_callback)) {}

RemoteActorRequest::~RemoteActorRequest() = default;

void RemoteActorRequest::Start() {
  if (!identity_manager_) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(completion_callback_), this, false));
    return;
  }

  access_token_fetcher_ =
      std::make_unique<signin::PrimaryAccountAccessTokenFetcher>(
          consumer_id_, identity_manager_,
          base::BindOnce(&RemoteActorRequest::OnAccessTokenFetchComplete,
                         base::Unretained(this)),
          signin::PrimaryAccountAccessTokenFetcher::Mode::kImmediate,
          signin::ConsentLevel::kSignin);
}

void RemoteActorRequest::OnAccessTokenFetchComplete(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    std::move(completion_callback_).Run(this, false);
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;
  resource_request->method = method_;
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer " + access_token_info.token);
  resource_request->headers.SetHeader(
      "X-Developer-Key", GaiaUrls::GetInstance()->oauth2_chrome_client_id());
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAccept,
                                      "application/json");

  loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                             traffic_annotation_);
  if (!post_data_.empty()) {
    loader_->AttachStringForUpload(post_data_, "application/json");
  }
  loader_->SetRetryOptions(kMaxRetries, network::SimpleURLLoader::RETRY_ON_5XX);
  loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&RemoteActorRequest::OnSimpleLoaderComplete,
                     base::Unretained(this)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void RemoteActorRequest::OnSimpleLoaderComplete(
    std::optional<std::string> response_body) {
  std::move(completion_callback_).Run(this, GetSuccess());
}

bool RemoteActorRequest::GetSuccess() const {
  if (!loader_ || loader_->NetError() != net::OK || !loader_->ResponseInfo() ||
      !loader_->ResponseInfo()->headers) {
    return false;
  }
  int response_code = loader_->ResponseInfo()->headers->response_code();
  return response_code == net::HTTP_OK || response_code == net::HTTP_NO_CONTENT;
}

}  // namespace password_manager
