// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"

#include "base/bind.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_status.h"

BitmapFetcher::BitmapFetcher(
    const GURL& url,
    BitmapFetcherDelegate* delegate,
    const net::NetworkTrafficAnnotationTag& traffic_annotation)
    : url_(url), delegate_(delegate), traffic_annotation_(traffic_annotation) {}

BitmapFetcher::~BitmapFetcher() {
}

void BitmapFetcher::Init(const std::string& referrer,
                         net::URLRequest::ReferrerPolicy referrer_policy,
                         network::mojom::CredentialsMode credentials_mode) {
  if (simple_loader_ != NULL)
    return;

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url_;
  resource_request->referrer = GURL(referrer);
  resource_request->referrer_policy = referrer_policy;
  resource_request->credentials_mode = credentials_mode;
  simple_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                    traffic_annotation_);
}

void BitmapFetcher::Start(network::mojom::URLLoaderFactory* loader_factory) {
  if (simple_loader_) {
    network::SimpleURLLoader::BodyAsStringCallback callback = base::BindOnce(
        &BitmapFetcher::OnSimpleLoaderComplete, base::Unretained(this));
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
  ImageDecoder::Start(this, *response_body);
}

// Methods inherited from ImageDecoder::ImageRequest.

void BitmapFetcher::OnImageDecoded(const SkBitmap& decoded_image) {
  // Report success.
  delegate_->OnFetchComplete(url_, &decoded_image);
}

void BitmapFetcher::OnDecodeImageFailed() {
  ReportFailure();
}

void BitmapFetcher::ReportFailure() {
  delegate_->OnFetchComplete(url_, NULL);
}
