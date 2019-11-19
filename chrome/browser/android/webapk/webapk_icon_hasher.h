// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_ICON_HASHER_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_ICON_HASHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/timer/timer.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {
class SimpleURLLoader;
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

// Downloads an icon and takes a Murmur2 hash of the downloaded image.
class WebApkIconHasher {
 public:
  using Murmur2HashCallback =
      base::Callback<void(const std::string& /* icon_murmur2_hash */)>;

  // Creates a self-owned WebApkIconHasher instance. The instance downloads
  // |icon_url| and calls |callback| with the Murmur2 hash of the downloaded
  // image. The hash is taken over the raw image bytes (no image
  // encoding/decoding beforehand). |callback| is called with an empty string if
  // the image cannot not be downloaded in time (e.g. 404 HTTP error code).
  static void DownloadAndComputeMurmur2Hash(
      network::mojom::URLLoaderFactory* url_loader_factory,
      const url::Origin& request_initiator,
      const GURL& icon_url,
      const Murmur2HashCallback& callback);

  static void DownloadAndComputeMurmur2HashWithTimeout(
      network::mojom::URLLoaderFactory* url_loader_factory,
      const url::Origin& request_initiator,
      const GURL& icon_url,
      int timeout_ms,
      const Murmur2HashCallback& callback);

 private:
  WebApkIconHasher(network::mojom::URLLoaderFactory* url_loader_factory,
                   const url::Origin& request_initiator,
                   const GURL& icon_url,
                   int timeout_ms,
                   const Murmur2HashCallback& callback);
  ~WebApkIconHasher();

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  // Called if downloading the icon takes too long.
  void OnDownloadTimedOut();

  // Calls |callback_| with |icon_murmur2_hash|. Also deletes the instance.
  void RunCallback(const std::string& icon_murmur2_hash);

  // Called with the image hash.
  Murmur2HashCallback callback_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Fails WebApkIconHasher if the download takes too long.
  base::OneShotTimer download_timeout_timer_;

  DISALLOW_COPY_AND_ASSIGN(WebApkIconHasher);
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_ICON_HASHER_H_
