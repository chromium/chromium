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

void OnGifMediaDownloaded(base::WeakPtr<const network::SimpleURLLoader> loader,
                          DownloadGifMediaToStringCallback callback,
                          std::unique_ptr<std::string> response_body) {
  if (loader && loader->NetError() == net::OK && response_body) {
    std::move(callback).Run(*response_body);
    return;
  }
  // TODO: b/325368650 - Determine how network errors should be handled.
  std::move(callback).Run(std::string());
}

// Creates a network request for GIF without starting the request.
std::unique_ptr<network::SimpleURLLoader> CreateGifMediaRequest(
    const GURL& url) {
  if (!IsValidGifMediaUrl(url)) {
    return nullptr;
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
  return network::SimpleURLLoader::Create(std::move(resource_request),
                                          kTrafficAnnotation);
}

// Downloads a gif or gif preview using `loader`. If the download is successful,
// the gif is passed to `callback` as a string of encoded bytes in gif or png
// format. Otherwise, `callback` is run with an empty string.
void DownloadGifMediaToString(
    base::WeakPtr<const network::SimpleURLLoader> loader,
    scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory,
    DownloadGifMediaToStringCallback callback) {
  if (!loader) {
    return;
  }

  // This is safe because `loader` is a WeakPtr to the non-const SimpleURLLoader
  // created by `CreateGifMediaRequest`.
  const_cast<network::SimpleURLLoader*>(loader.get())
      ->DownloadToString(
          shared_url_loader_factory.get(),
          base::BindOnce(&OnGifMediaDownloaded, loader, std::move(callback)),
          network::SimpleURLLoader::kMaxBoundedStringDownloadSize);
}

void RunImmediatelyOrWithDelay(base::OnceClosure closure,
                               base::TimeDelta delay) {
  if (delay.is_zero()) {
    std::move(closure).Run();
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, std::move(closure), delay);
  }
}

// Returns the delay for the request with `rank`.
// The delay should increase with `rank` to encourage lower rank requests to
// finish first and avoid congestion.
base::TimeDelta GetRequestDelay(size_t rank) {
  // The top 4 results should be fetched instantly, since they are likely to be
  // visible above the fold.
  if (rank < 4) {
    return base::Seconds(0);
  }
  // The remaining results can be fetched later since they are likely to be
  // below the fold.
  return base::Milliseconds(200) + rank * base::Milliseconds(100);
}

}  // namespace

QuickInsertAssetFetcherImpl::QuickInsertAssetFetcherImpl(
    QuickInsertAssetFetcherImplDelegate* delegate)
    : delegate_(delegate) {}

QuickInsertAssetFetcherImpl::~QuickInsertAssetFetcherImpl() = default;

std::unique_ptr<network::SimpleURLLoader>
QuickInsertAssetFetcherImpl::FetchGifFromUrl(
    const GURL& url,
    size_t rank,
    QuickInsertGifFetchedCallback callback) {
  std::unique_ptr<network::SimpleURLLoader> loader = CreateGifMediaRequest(url);
  if (loader == nullptr) {
    std::move(callback).Run({});
    return nullptr;
  }
  RunImmediatelyOrWithDelay(
      base::BindOnce(&DownloadGifMediaToString, loader->GetWeakPtr(),
                     delegate_->GetSharedURLLoaderFactory(),
                     base::BindOnce(&image_util::DecodeAnimationData,
                                    std::move(callback))),
      GetRequestDelay(rank) + base::Milliseconds(200));
  return loader;
}

std::unique_ptr<network::SimpleURLLoader>
QuickInsertAssetFetcherImpl::FetchGifPreviewImageFromUrl(
    const GURL& url,
    size_t rank,
    QuickInsertImageFetchedCallback callback) {
  std::unique_ptr<network::SimpleURLLoader> loader = CreateGifMediaRequest(url);
  if (loader == nullptr) {
    std::move(callback).Run({});
    return nullptr;
  }
  RunImmediatelyOrWithDelay(
      base::BindOnce(
          &DownloadGifMediaToString, loader->GetWeakPtr(),
          delegate_->GetSharedURLLoaderFactory(),
          base::BindOnce(&image_util::DecodeImageData, std::move(callback),
                         data_decoder::mojom::ImageCodec::kDefault)),
      GetRequestDelay(rank));
  return loader;
}

void QuickInsertAssetFetcherImpl::FetchFileThumbnail(
    const base::FilePath& path,
    const gfx::Size& size,
    FetchFileThumbnailCallback callback) {
  delegate_->FetchFileThumbnail(path, size, std::move(callback));
}

}  // namespace ash
