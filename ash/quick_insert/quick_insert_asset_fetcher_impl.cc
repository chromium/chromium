// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/quick_insert/quick_insert_asset_fetcher_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "ash/public/cpp/image_util.h"
#include "ash/quick_insert/quick_insert_asset_fetcher.h"
#include "ash/quick_insert/quick_insert_asset_fetcher_impl_delegate.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom-shared.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace ash {
namespace {

using DownloadGifMediaToStringCallback =
    base::OnceCallback<void(const std::string& gif_media_data)>;

bool IsValidGifMediaUrl(const GURL& url) {
  // TODO: b/323784358 - Check requirements for validating GIF urls.
  return url.DomainIs("media.tenor.com") && url.SchemeIs(url::kHttpsScheme);
}

void OnGifMediaDownloaded(
    DownloadGifMediaToStringCallback callback,
    std::unique_ptr<network::SimpleURLLoader> simple_loader,
    std::unique_ptr<std::string> response_body) {
  if (simple_loader->NetError() == net::OK && response_body) {
    std::move(callback).Run(*response_body);
    return;
  }
  // TODO: b/325368650 - Determine how network errors should be handled.
  std::move(callback).Run(std::string());
}

// Downloads a gif or gif preview from `url`. If the download is successful,
// the gif is passed to `callback` as a string of encoded bytes in gif or png
// format. Otherwise, `callback` is run with an empty string.
void DownloadGifMediaToString(
    const GURL& url,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    DownloadGifMediaToStringCallback callback) {
  if (!IsValidGifMediaUrl(url)) {
    // TODO: b/325368650 - Determine how invalid urls should be handled.
    std::move(callback).Run(std::string());
    return;
  }

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = url;
  resource_request->method = "GET";
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;

  constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
      net::DefineNetworkTrafficAnnotation(
          "chromeos_quick_insert_gif_media_fetcher",
          R"(
      semantics {
        sender: "ChromeOS Quick Insert"
        description:
          "Fetches a GIF or GIF preview from Tenor for the specified url. This "
          "is used to show GIFs and GIF preview images in ChromeOS Quick "
          "Insert, which users can select to insert the GIF into supported "
          "textfields."
        trigger:
          "Triggered when the user opens ChromeOS Quick Insert."
        data:
          "A GIF ID to specify the GIF to fetch."
        destination: GOOGLE_OWNED_SERVICE
        internal {
          contacts {
              email: "e14s-eng@google.com"
          }
        }
        user_data {
          type: NONE
        }
        last_reviewed: "2024-10-15"
      }
      policy {
        cookies_allowed: NO
        setting:
          "No setting. Users must take explicit action to trigger the feature."
        policy_exception_justification:
          "Not implemented, not considered useful. This request is part of a "
          "flow which is user-initiated."
      }
  )");
  auto loader = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 kTrafficAnnotation);
  auto* loader_ptr = loader.get();
  loader_ptr->DownloadToString(
      shared_url_loader_factory.get(),
      base::BindOnce(&OnGifMediaDownloaded, std::move(callback),
                     std::move(loader)),
      network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

base::TimeDelta GetRetryJitter() {
  return base::RandTimeDelta(base::Milliseconds(100), base::Seconds(1));
}

}  // namespace

QuickInsertAssetFetcherImpl::QuickInsertAssetFetcherImpl(
    QuickInsertAssetFetcherImplDelegate* delegate)
    : delegate_(delegate) {}

QuickInsertAssetFetcherImpl::~QuickInsertAssetFetcherImpl() = default;

void QuickInsertAssetFetcherImpl::FetchGifFromUrl(
    const GURL& url,
    QuickInsertGifFetchedCallback callback) {
  if (pending_network_requests_ >= kMaxPendingNetworkRequests) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&QuickInsertAssetFetcherImpl::FetchGifFromUrl,
                       weak_ptr_factory_.GetWeakPtr(), url,
                       std::move(callback)),
        GetRetryJitter());
    return;
  }

  ++pending_network_requests_;
  DownloadGifMediaToString(
      url, delegate_->GetSharedURLLoaderFactory(),
      base::BindOnce(
          &image_util::DecodeAnimationData,
          std::move(callback).Then(base::BindOnce(
              &QuickInsertAssetFetcherImpl::OnNetworkRequestCompleted,
              weak_ptr_factory_.GetWeakPtr()))));
}

void QuickInsertAssetFetcherImpl::FetchGifPreviewImageFromUrl(
    const GURL& url,
    QuickInsertImageFetchedCallback callback) {
  if (pending_network_requests_ >= kMaxPendingNetworkRequests) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &QuickInsertAssetFetcherImpl::FetchGifPreviewImageFromUrl,
            weak_ptr_factory_.GetWeakPtr(), url, std::move(callback)),
        GetRetryJitter());
    return;
  }

  ++pending_network_requests_;
  DownloadGifMediaToString(
      url, delegate_->GetSharedURLLoaderFactory(),
      base::BindOnce(
          &image_util::DecodeImageData,
          std::move(callback).Then(base::BindOnce(
              &QuickInsertAssetFetcherImpl::OnNetworkRequestCompleted,
              weak_ptr_factory_.GetWeakPtr())),
          data_decoder::mojom::ImageCodec::kDefault));
}

void QuickInsertAssetFetcherImpl::FetchFileThumbnail(
    const base::FilePath& path,
    const gfx::Size& size,
    FetchFileThumbnailCallback callback) {
  delegate_->FetchFileThumbnail(path, size, std::move(callback));
}

void QuickInsertAssetFetcherImpl::OnNetworkRequestCompleted() {
  --pending_network_requests_;
}

}  // namespace ash
