// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_NTP_JSON_FETCHER_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_NTP_JSON_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/explore_sites/catalog.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace content {
class BrowserContext;
}

namespace network {
class SimpleURLLoader;
}

namespace explore_sites {

// A class that fetches a JSON formatted response from a server and uses a
// sandboxed utility process to parse it to a DictionaryValue.
class NTPJsonFetcher {
 public:
  // Callback to pass back the parsed json dictionary returned from the server.
  // Invoked with |nullptr| if there is an error.
  typedef base::OnceCallback<void(std::unique_ptr<NTPCatalog>)> Callback;

  explicit NTPJsonFetcher(content::BrowserContext* browser_context);
  ~NTPJsonFetcher();

  // Starts to fetch results for the given |query_url|.
  void Start(Callback callback);
  void Stop();

 private:
  // Invoked from SimpleURLLoader after download is complete.
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);
  // Callback for DataDecoder.
  void OnJsonParse(data_decoder::DataDecoder::ValueOrError result);
  void OnJsonParseError(const std::string& error);

  Callback callback_;
  content::BrowserContext* browser_context_;
  std::unique_ptr<network::SimpleURLLoader> simple_loader_;
  base::WeakPtrFactory<NTPJsonFetcher> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(NTPJsonFetcher);
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_NTP_JSON_FETCHER_H_
