// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_IMAGE_DOWNLOADER_H_
#define ASH_PUBLIC_CPP_IMAGE_DOWNLOADER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/functional/callback_forward.h"

class GURL;
class AccountId;

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace net {
class HttpRequestHeaders;
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace ash {

// Interface for a class which is responsible for downloading images in ash.
class ASH_PUBLIC_EXPORT ImageDownloader {
 public:
  static ImageDownloader* Get();

  using DownloadCallback = base::OnceCallback<void(const gfx::ImageSkia&)>;

  ImageDownloader(const ImageDownloader&) = delete;
  ImageDownloader& operator=(const ImageDownloader&) = delete;

  // Downloads the image found at |url| for the primary profile. On completion,
  // |callback| is run with the downloaded |image|. In the event that the
  // download attempt fails, a nullptr image will be returned.
  virtual void Download(const GURL& url,
                        const net::NetworkTrafficAnnotationTag& annotation_tag,
                        const AccountId& account_id,
                        DownloadCallback callback) = 0;

  // Additionally with this method, you can specify extra HTTP request headers
  // sent with the download request.
  virtual void Download(const GURL& url,
                        const net::NetworkTrafficAnnotationTag& annotation_tag,
                        const AccountId& account_id,
                        const net::HttpRequestHeaders& additional_headers,
                        DownloadCallback callback) = 0;

 protected:
  ImageDownloader();
  virtual ~ImageDownloader();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_IMAGE_DOWNLOADER_H_
