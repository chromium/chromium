// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/enterprise/platform_auth/url_session_url_loader.h"

#include <Foundation/Foundation.h>

#include <cstdint>

#include "base/apple/foundation_util.h"
#include "base/byte_size.h"
#include "base/check.h"
#include "base/check_is_test.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_proxying_url_loader_factory.h"
#include "chrome/browser/enterprise/platform_auth/url_session_helper.h"
#include "net/base/net_errors.h"
#include "net/http/http_version.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace enterprise_auth {

namespace {

constexpr base::TimeDelta kTimeout = base::Seconds(30);
constexpr base::ByteSize kDataSizeLimit = base::KiBU(128);

}  // namespace

URLSessionURLLoader::URLSessionURLLoader() = default;

URLSessionURLLoader::~URLSessionURLLoader() {
  if (task_) {
    [task_ cancel];
  }
}

// static
void URLSessionURLLoader::CreateAndStart(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_info) {
  // Lifetime of this class is self-managed, see url_session_url_loader.h for
  // more details.
  URLSessionURLLoader* url_loader = new URLSessionURLLoader();
  url_loader->Start(request, std::move(loader), std::move(client_info));
}

void URLSessionURLLoader::Start(
    const network::ResourceRequest& request,
    mojo::PendingReceiver<network::mojom::URLLoader> loader,
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_info_remote) {
  DCHECK(ProxyingURLLoaderFactory::IsOktaSSORequest(request))
      << "URLSessionURLLoader is meant to be used only for Okta SSO.";

  client_.Bind(std::move(client_info_remote));
  receiver_.Bind(std::move(loader));

  // base::Unretained is safe because the callbacks are owned by this object.
  receiver_.set_disconnect_handler(base::BindOnce(
      &URLSessionURLLoader::OnReceiverDisconnect, base::Unretained(this)));
  client_.set_disconnect_handler(base::BindOnce(
      &URLSessionURLLoader::OnClientDisconnect, base::Unretained(this)));

  request_start_ = base::TimeTicks::Now();

  NSURLRequest* ns_request =
      url_session_helper::ConvertResourceRequest(request, kTimeout);
  NSURLSession* session = [NSURLSession sharedSession];
  if (session_override_) {
    CHECK_IS_TEST();
    session = session_override_;
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::SequencedTaskRunner::GetCurrentDefault();
  auto weak_ptr = weak_ptr_factory_.GetWeakPtr();
  // This completionHandler will run on Apple's network thread.
  // It captures task_runner and weak_ptr by copy.
  task_ = [session
      dataTaskWithRequest:ns_request
        completionHandler:^(NSData* data, NSURLResponse* response,
                            NSError* error) {
          if (error || !response) {
            task_runner->PostTask(
                FROM_HERE, base::BindOnce(&URLSessionURLLoader::OnRequestFailed,
                                          weak_ptr));
          } else {
            task_runner->PostTask(
                FROM_HERE,
                base::BindOnce(&URLSessionURLLoader::OnRequestComplete,
                               weak_ptr, response, data));
          }
        }];

  // Starts the request asynchronously.
  [task_ resume];
}

void URLSessionURLLoader::OnRequestComplete(NSURLResponse* response,
                                            NSData* ns_data) {
  task_ = nil;
  if (!client_) {
    // At this point, it is possible for |client_| to have disconnected, but
    // the callback might still be pending in the task queue.
    return;
  }

  // |response| is guarenteed to be valid.
  auto head = url_session_helper::ConvertNSURLResponse(response);
  head->request_start = request_start_;
  head->response_start = base::TimeTicks::Now();

  mojo::ScopedDataPipeConsumerHandle consumer_handle;
  if (ns_data && ns_data.length > 0) {
    if (ns_data.length > kDataSizeLimit.InBytes()) {
      client_->OnComplete(
          network::URLLoaderCompletionStatus(net::ERR_FILE_TOO_BIG));
      DisconnectAndDelete();
      return;
    }

    const base::span<const uint8_t> data = base::apple::NSDataToSpan(ns_data);
    mojo::ScopedDataPipeProducerHandle producer_handle;
    if (mojo::CreateDataPipe(data.size(), producer_handle, consumer_handle) !=
        MOJO_RESULT_OK) {
      client_->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      DisconnectAndDelete();
      return;
    }
    if (producer_handle->WriteAllData(data) != MOJO_RESULT_OK) {
      client_->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
      DisconnectAndDelete();
      return;
    }
  }

  client_->OnReceiveResponse(std::move(head), std::move(consumer_handle),
                             std::nullopt);
  client_->OnComplete(network::URLLoaderCompletionStatus(net::OK));
  DisconnectAndDelete();
}

void URLSessionURLLoader::OnRequestFailed() {
  task_ = nil;
  if (!client_) {
    // At this point, it is possible for |client_| to have disconnected, but
    // the callback might still be pending in the task queue.
    return;
  }
  client_->OnComplete(network::URLLoaderCompletionStatus(net::ERR_FAILED));
  DisconnectAndDelete();
}

void URLSessionURLLoader::OnClientDisconnect() {
  DisconnectAndDelete();
}

void URLSessionURLLoader::OnReceiverDisconnect() {
  // The receiver might have disconnected but the client might still be waiting
  // for the response, so we don't delete and disconnect just yet.
  receiver_.reset();
}

void URLSessionURLLoader::DisconnectAndDelete() {
  client_.reset();
  receiver_.reset();
  delete this;
}

// We let URLSession follow redirects and only get the final result.
void URLSessionURLLoader::FollowRedirect(
    const std::vector<std::string>& removed_headers,
    const ::net::HttpRequestHeaders& modified_headers,
    const ::net::HttpRequestHeaders& modified_cors_exempt_headers,
    const std::optional<::GURL>& new_url) {
  NOTREACHED();
}

// Does not apply to URLSession.
void URLSessionURLLoader::SetPriority(net::RequestPriority priority,
                                      int32_t intra_priority_value) {}

}  // namespace enterprise_auth
