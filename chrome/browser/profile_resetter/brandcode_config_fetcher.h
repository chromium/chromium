// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_BRANDCODE_CONFIG_FETCHER_H_
#define CHROME_BROWSER_PROFILE_RESETTER_BRANDCODE_CONFIG_FETCHER_H_

#include <memory>
#include <string>
#include <utility>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

class BrandcodedDefaultSettings;
class GURL;

namespace network {
class SimpleURLLoader;
namespace mojom {
class URLLoaderFactory;
}
}  // namespace network

// BrandcodeConfigFetcher fetches and parses the xml containing the brandcoded
// default settings. Caller should provide a FetchCallback.
class BrandcodeConfigFetcher {
 public:
  typedef base::OnceCallback<void()> FetchCallback;

  BrandcodeConfigFetcher(network::mojom::URLLoaderFactory* url_loader_factory,
                         FetchCallback callback,
                         const GURL& url,
                         const std::string& brandcode);

  BrandcodeConfigFetcher(const BrandcodeConfigFetcher&) = delete;
  BrandcodeConfigFetcher& operator=(const BrandcodeConfigFetcher&) = delete;

  ~BrandcodeConfigFetcher();

  bool IsActive() const { return !!simple_url_loader_; }

  std::unique_ptr<BrandcodedDefaultSettings> GetSettings() {
    return std::move(default_settings_);
  }

  // Sets the new callback. The previous one won't be called.
  void SetCallback(FetchCallback callback);

 private:
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);
  void OnXmlConfigParsed(
      data_decoder::DataDecoder::ValueOrError value_or_error);

  void OnDownloadTimeout();

  // Timer that enforces a timeout on the attempt to download the
  // config file.
  base::OneShotTimer download_timer_;

  // |fetch_callback_| called when fetching succeeded or failed.
  FetchCallback fetch_callback_;

  // Helper to fetch the online config file.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Fetched settings.
  std::unique_ptr<BrandcodedDefaultSettings> default_settings_;

  base::WeakPtrFactory<BrandcodeConfigFetcher> weak_ptr_factory_;
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_BRANDCODE_CONFIG_FETCHER_H_
