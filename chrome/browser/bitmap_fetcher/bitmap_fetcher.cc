// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/base/data_url.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/cpp/resource_request.h"
#include "url/url_constants.h"

BitmapFetcher::BitmapFetcher(
    const GURL& url,
    BitmapFetcherDelegate* delegate,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : BitmapFetcher(url, delegate, traffic_annotation, nullptr) {}

BitmapFetcher::BitmapFetcher(
    const GURL& url,
    BitmapFetcherDelegate* delegate,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    data_decoder::DataDecoder* data_decoder)
    : ImageDecoder::ImageRequest(data_decoder),
      url_(url),
      delegate_(delegate),
      traffic_annotation_(traffic_annotation) {}

BitmapFetcher::~BitmapFetcher() = default;

void BitmapFetcher::Init(net::ReferrerPolicy referrer_policy,
                         network::mojom::CredentialsMode credentials_mode,
                         const net::HttpRequestHeaders& additional_headers,
                         const url::Origin& initiator) {
  if (simple_loader_ != nullptr)
    return;

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;
  resource_request->referrer_policy = referrer_policy;
  resource_request->credentials_mode = credentials_mode;
  resource_request->headers.MergeFrom(additional_headers);
  resource_request->request_initiator = initiator;
  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation_);
}

void BitmapFetcher::Start(network::mojom::URLLoaderFactory* loader_factory) {
  network::SimpleURLLoader::BodyAsStringCallbackDeprecated callback =
      base::BindOnce(&BitmapFetcher::OnSimpleLoaderComplete,
                     weak_factory_.GetWeakPtr());

  // Early exit to handle data URLs.
  if (url_.SchemeIs(url::kDataScheme)) {
    std::string mime_type, charset, data;
    std::unique_ptr<std::string> response_body;
    if (net::DataURL::Parse(url_, &mime_type, &charset, &data))
      response_body = std::make_unique<std::string>(std::move(data));

    // Post a task to maintain our guarantee that the delegate will only be
    // called asynchronously.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, BindOnce(std::move(callback), std::move(response_body)));
    return;
  }

  if (simple_loader_) {
    simple_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        loader_factory, std::move(callback));
  }
}

void BitmapFetcher::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  if (!response_body) {
    ReportFailure();
    return;
  }

  // Call start to begin decoding.  The ImageDecoder will call OnImageDecoded
  // with the data when it is done.
  ImageDecoder::Start(this, std::move(*response_body));
}

// Methods inherited from ImageDecoder::ImageRequest.

void BitmapFetcher::OnImageDecoded(const SkBitmap& decoded_image) {
  delegate_->OnFetchComplete(url_, &decoded_image);
}

void BitmapFetcher::OnDecodeImageFailed() {
  ReportFailure();
}

void BitmapFetcher::ReportFailure() {
  delegate_->OnFetchComplete(url_, nullptr);
}
