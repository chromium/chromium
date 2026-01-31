// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_http_service_handler.h"

#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/access_token_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

class DevToolsHttpServiceHandler::DevToolsStreamConsumer
    : public network::SimpleURLLoaderStreamConsumer {
 public:
  DevToolsStreamConsumer(DevToolsHttpServiceHandler::StreamWriter stream_writer,
                         DevToolsHttpServiceHandler::Callback callback,
                         network::SimpleURLLoader* loader,
                         base::OnceClosure cleanup)
      : stream_writer_(std::move(stream_writer)),
        callback_(std::move(callback)),
        loader_(loader),
        cleanup_(std::move(cleanup)) {
    CHECK(loader_);
  }

  ~DevToolsStreamConsumer() override = default;

  void OnDataReceived(std::string_view chunk,
                      base::OnceClosure resume) override {
    stream_writer_.Run(chunk);
    std::move(resume).Run();
  }

  void OnComplete(bool success) override {
    CHECK(loader_);
    auto result = std::make_unique<DevToolsHttpServiceHandler::Result>();
    result->net_error = loader_->NetError();
    if (loader_->ResponseInfo() && loader_->ResponseInfo()->headers) {
      result->http_status = loader_->ResponseInfo()->headers->response_code();
    }

    if (result->net_error != net::OK) {
      result->error = DevToolsHttpServiceHandler::Result::Error::kNetworkError;
    } else if (result->http_status == -1) {
      result->error = DevToolsHttpServiceHandler::Result::Error::kNetworkError;
    } else if (result->http_status < 200 || result->http_status >= 300) {
      result->error = DevToolsHttpServiceHandler::Result::Error::kHttpError;
    } else if (!success) {
      // There was an error and we don't know why, we default to network
      // error for such cases.
      result->error = DevToolsHttpServiceHandler::Result::Error::kNetworkError;
    }

    // Run completion callback
    std::move(callback_).Run(std::move(result));

    // The cleanup callback destroys the ActiveStreamRequest, which owns this
    // Consumer. We must post the task to ensure destruction happens
    // asynchronously after the current stack frame unwinds, avoiding
    // a potential use-after-free of `this`.
    if (cleanup_) {
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, std::move(cleanup_));
    }
  }

  void OnRetry(base::OnceClosure start_retry) override { NOTREACHED(); }

 private:
  DevToolsHttpServiceHandler::StreamWriter stream_writer_;
  DevToolsHttpServiceHandler::Callback callback_;
  raw_ptr<network::SimpleURLLoader> loader_ = nullptr;
  base::OnceClosure cleanup_;
};

DevToolsHttpServiceHandler::Result::Result() = default;
DevToolsHttpServiceHandler::Result::~Result() = default;
DevToolsHttpServiceHandler::Result::Result(Result&&) = default;
DevToolsHttpServiceHandler::Result&
DevToolsHttpServiceHandler::Result::operator=(Result&&) = default;

DevToolsHttpServiceHandler::ActiveStreamRequest::ActiveStreamRequest() =
    default;
DevToolsHttpServiceHandler::ActiveStreamRequest::~ActiveStreamRequest() =
    default;
DevToolsHttpServiceHandler::ActiveStreamRequest::ActiveStreamRequest(
    ActiveStreamRequest&&) = default;
DevToolsHttpServiceHandler::ActiveStreamRequest&
DevToolsHttpServiceHandler::ActiveStreamRequest::operator=(
    ActiveStreamRequest&&) = default;

DevToolsHttpServiceHandler::~DevToolsHttpServiceHandler() = default;
DevToolsHttpServiceHandler::DevToolsHttpServiceHandler() = default;

void DevToolsHttpServiceHandler::Request(
    Profile* profile,
    const DevToolsDispatchHttpRequestParams& params,
    std::optional<StreamWriter> stream_writer,
    Callback callback) {
  CanMakeRequest(profile,
                 base::BindOnce(&DevToolsHttpServiceHandler::OnValidationDone,
                                weak_factory_.GetWeakPtr(), std::move(callback),
                                profile, std::move(stream_writer), params));
}

void DevToolsHttpServiceHandler::CanMakeRequest(
    Profile* profile,
    base::OnceCallback<void(bool success)> callback) {
  std::move(callback).Run(profile && !profile->IsOffTheRecord());
}

void DevToolsHttpServiceHandler::OnValidationDone(
    Callback callback,
    Profile* profile,
    std::optional<StreamWriter> stream_writer,
    const DevToolsDispatchHttpRequestParams& params,
    bool validation_success) {
  if (!validation_success) {
    auto result = std::make_unique<Result>();
    result->error = Result::Error::kValidationFailed;
    std::move(callback).Run(std::move(result));
    return;
  }

  auto* identity_manager = IdentityManagerFactory::GetForProfile(profile);

  auto fetcher_id = base::UnguessableToken::Create();
  auto access_token_fetcher =
      identity_manager->CreateAccessTokenFetcherForAccount(
          identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin),
          OAuthConsumerId(),
          base::BindOnce(&DevToolsHttpServiceHandler::OnTokenFetched,
                         weak_factory_.GetWeakPtr(), std::move(callback),
                         profile, std::move(stream_writer), params, fetcher_id),
          signin::AccessTokenFetcher::Mode::kImmediate);
  access_token_fetchers_.insert({
      fetcher_id,
      std::move(access_token_fetcher),
  });
}

void DevToolsHttpServiceHandler::OnTokenFetched(
    Callback callback,
    Profile* profile,
    std::optional<StreamWriter> stream_writer,
    const DevToolsDispatchHttpRequestParams& params,
    base::UnguessableToken fetcher_id,
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  access_token_fetchers_.erase(fetcher_id);
  if (error.state() != GoogleServiceAuthError::NONE) {
    auto result = std::make_unique<Result>();
    result->error = Result::Error::kTokenFetchFailed;
    result->error_detail = error.ToString();
    std::move(callback).Run(std::move(result));
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  GURL url = BaseURL().Resolve(params.path);
  for (const auto& pair : params.query_params) {
    const std::string& key = pair.first;
    for (const std::string& value : pair.second) {
      url = net::AppendQueryParameter(url, key, value);
    }
  }
  resource_request->url = url;
  resource_request->method = params.method;
  resource_request->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                      "Bearer " + access_token_info.token);

  auto simple_url_loader = network::SimpleURLLoader::Create(
      std::move(resource_request), NetworkTrafficAnnotationTag());
  simple_url_loader->SetAllowHttpErrorResults(true);

  if (params.body.has_value()) {
    simple_url_loader->AttachStringForUpload(params.body.value(),
                                             "application/json");
  }

  if (stream_writer) {
    auto token = base::UnguessableToken::Create();
    auto& request = active_streams_[token];
    request.loader = std::move(simple_url_loader);

    auto cleanup_callback = base::BindOnce(
        [](base::WeakPtr<DevToolsHttpServiceHandler> self,
           base::UnguessableToken token) {
          if (self) {
            self->active_streams_.erase(token);
          }
        },
        weak_factory_.GetWeakPtr(), token);
    auto consumer = std::make_unique<DevToolsStreamConsumer>(
        std::move(*stream_writer), std::move(callback), request.loader.get(),
        std::move(cleanup_callback));
    request.consumer = std::move(consumer);

    request.loader->DownloadAsStream(
        profile->GetDefaultStoragePartition()
            ->GetURLLoaderFactoryForBrowserProcess()
            .get(),
        request.consumer.get());
    return;
  }

  network::SimpleURLLoader* loader_ptr = simple_url_loader.get();
  loader_ptr->DownloadToString(
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess()
          .get(),
      base::BindOnce(&DevToolsHttpServiceHandler::OnRequestComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback),
                     std::move(simple_url_loader)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void DevToolsHttpServiceHandler::OnRequestComplete(
    Callback callback,
    std::unique_ptr<network::SimpleURLLoader> simple_url_loader,
    std::optional<std::string> response_body) {
  auto result = std::make_unique<Result>();
  result->net_error = simple_url_loader->NetError();
  result->response_body = std::move(response_body);
  if (simple_url_loader->ResponseInfo() &&
      simple_url_loader->ResponseInfo()->headers) {
    result->http_status =
        simple_url_loader->ResponseInfo()->headers->response_code();
  }

  if (result->net_error != net::OK) {
    result->error = Result::Error::kNetworkError;
  } else if (result->http_status < net::HTTP_OK ||
             result->http_status >= net::HTTP_MULTIPLE_CHOICES) {
    result->error = Result::Error::kHttpError;
  }

  std::move(callback).Run(std::move(result));
}
