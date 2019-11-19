// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_icon_hasher.h"

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/data_url.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "third_party/smhasher/src/MurmurHash2.h"

namespace {

// The seed to use when taking the murmur2 hash of the icon.
const uint64_t kMurmur2HashSeed = 0;

// The default number of milliseconds to wait for the icon download to complete.
const int kDownloadTimeoutInMilliseconds = 60000;

// Computes Murmur2 hash of |raw_image_data|.
std::string ComputeMurmur2Hash(const std::string& raw_image_data) {
  // WARNING: We are running in the browser process. |raw_image_data| is the
  // image's raw, unsanitized bytes from the web. |raw_image_data| may contain
  // malicious data. Decoding unsanitized bitmap data to an SkBitmap in the
  // browser process is a security bug.
  uint64_t hash = MurmurHash64A(raw_image_data.data(), raw_image_data.size(),
                                kMurmur2HashSeed);
  return base::NumberToString(hash);
}

}  // anonymous namespace

// static
void WebApkIconHasher::DownloadAndComputeMurmur2Hash(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const url::Origin& request_initiator,
    const GURL& icon_url,
    const Murmur2HashCallback& callback) {
  DownloadAndComputeMurmur2HashWithTimeout(
      url_loader_factory, request_initiator, icon_url,
      kDownloadTimeoutInMilliseconds, callback);
}

// static
void WebApkIconHasher::DownloadAndComputeMurmur2HashWithTimeout(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const url::Origin& request_initiator,
    const GURL& icon_url,
    int timeout_ms,
    const Murmur2HashCallback& callback) {
  if (!icon_url.is_valid()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  base::BindOnce(callback, ""));
    return;
  }

  if (icon_url.SchemeIs(url::kDataScheme)) {
    std::string mime_type, char_set, data;
    std::string hash;
    if (net::DataURL::Parse(icon_url, &mime_type, &char_set, &data) &&
        !data.empty()) {
      hash = ComputeMurmur2Hash(data);
    }
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(callback, hash));
    return;
  }

  // The icon hasher will delete itself when it is done.
  new WebApkIconHasher(url_loader_factory, request_initiator, icon_url,
                       timeout_ms, callback);
}

WebApkIconHasher::WebApkIconHasher(
    network::mojom::URLLoaderFactory* url_loader_factory,
    const url::Origin& request_initiator,
    const GURL& icon_url,
    int timeout_ms,
    const Murmur2HashCallback& callback)
    : callback_(callback) {
  DCHECK(url_loader_factory);

  download_timeout_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(timeout_ms),
      base::Bind(&WebApkIconHasher::OnDownloadTimedOut,
                 base::Unretained(this)));

  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->request_initiator = request_initiator;
  resource_request->url = icon_url;
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request),
      TRAFFIC_ANNOTATION_WITHOUT_PROTO("webapk icon hasher"));
  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory,
      base::BindOnce(&WebApkIconHasher::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

WebApkIconHasher::~WebApkIconHasher() {}

void WebApkIconHasher::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  download_timeout_timer_.Stop();

  // Check for non-empty body in case of HTTP 204 (no content) response.
  if (!response_body || response_body->empty()) {
    RunCallback("");
    return;
  }

  // WARNING: We are running in the browser process. |*response_body| is the
  // image's raw, unsanitized bytes from the web. |*response_body| may contain
  // malicious data. Decoding unsanitized bitmap data to an SkBitmap in the
  // browser process is a security bug.
  RunCallback(ComputeMurmur2Hash(*response_body));
}

void WebApkIconHasher::OnDownloadTimedOut() {
  simple_url_loader_.reset();

  RunCallback("");
}

void WebApkIconHasher::RunCallback(const std::string& icon_murmur2_hash) {
  callback_.Run(icon_murmur2_hash);
  delete this;
}
