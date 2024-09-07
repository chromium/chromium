// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/image_downloader/image_downloader_impl.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "net/url_request/referrer_policy.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/fetch_api.mojom-shared.h"
#include "ui/gfx/image/image_skia.h"

namespace {

// DownloadTask ----------------------------------------------------------------

class DownloadTask : public BitmapFetcherDelegate {
 public:
  DownloadTask(const GURL& url,
               const net::NetworkTrafficAnnotationTag& annotation_tag,
               const AccountId& account_id,
               const net::HttpRequestHeaders& additional_headers,
               ash::ImageDownloader::DownloadCallback callback)
      : callback_(std::move(callback)) {
    StartTask(url, annotation_tag, account_id, additional_headers);
  }

  DownloadTask(const DownloadTask&) = delete;
  DownloadTask& operator=(const DownloadTask&) = delete;
  ~DownloadTask() override = default;

  // BitmapFetcherDelegate:
  void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override {
    std::move(callback_).Run(bitmap
                                 ? gfx::ImageSkia::CreateFrom1xBitmap(*bitmap)
                                 : gfx::ImageSkia());
    delete this;
  }

 private:
  void StartTask(const GURL& url,
                 const net::NetworkTrafficAnnotationTag& annotation_tag,
                 const AccountId& account_id,
                 const net::HttpRequestHeaders& additional_headers) {
    Profile* profile =
        ash::ProfileHelper::Get()->GetProfileByAccountId(account_id);
    if (!profile) {
      std::move(callback_).Run(gfx::ImageSkia());
      return;
    }

    bitmap_fetcher_ =
        std::make_unique<BitmapFetcher>(url, this, annotation_tag);

    bitmap_fetcher_->Init(net::ReferrerPolicy::NEVER_CLEAR,
                          network::mojom::CredentialsMode::kOmit,
                          additional_headers);

    bitmap_fetcher_->Start(profile->GetURLLoaderFactory().get());
  }

  ash::ImageDownloader::DownloadCallback callback_;
  std::unique_ptr<BitmapFetcher> bitmap_fetcher_;
};

}  // namespace

// ImageDownloaderImpl ----------------------------------------------------

ImageDownloaderImpl::ImageDownloaderImpl() = default;

ImageDownloaderImpl::~ImageDownloaderImpl() = default;

void ImageDownloaderImpl::Download(
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    const AccountId& account_id,
    ash::ImageDownloader::DownloadCallback callback) {
  Download(url, annotation_tag, account_id, /*additional_headers=*/{},
           std::move(callback));
}

void ImageDownloaderImpl::Download(
    const GURL& url,
    const net::NetworkTrafficAnnotationTag& annotation_tag,
    const AccountId& account_id,
    const net::HttpRequestHeaders& additional_headers,
    ash::ImageDownloader::DownloadCallback callback) {
  DCHECK(account_id.is_valid());
  // The download task will delete itself upon task completion.
  new DownloadTask(url, annotation_tag, account_id, additional_headers,
                   std::move(callback));
}
