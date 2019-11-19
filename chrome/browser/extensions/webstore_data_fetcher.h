// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/gurl.h"

namespace network {
class SimpleURLLoader;
namespace mojom {
class URLLoaderFactory;
}  // namespace mojom
}  // namespace network

namespace extensions {

class WebstoreDataFetcherDelegate;

// WebstoreDataFetcher fetches web store data and parses it into a
// DictionaryValue.
class WebstoreDataFetcher : public base::SupportsWeakPtr<WebstoreDataFetcher> {
 public:
  WebstoreDataFetcher(WebstoreDataFetcherDelegate* delegate,
                      const GURL& referrer_url,
                      const std::string webstore_item_id);
  ~WebstoreDataFetcher();

  void Start(network::mojom::URLLoaderFactory* url_loader_factory);

  void set_max_auto_retries(int max_retries) {
    max_auto_retries_ = max_retries;
  }

 private:
  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError result);
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  WebstoreDataFetcherDelegate* delegate_;
  GURL referrer_url_;
  std::string id_;
  std::string post_data_;

  // For fetching webstore JSON data.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Maximum auto retry times on server 5xx error or ERR_NETWORK_CHANGED.
  // Default is 0 which means to use the URLFetcher default behavior.
  int max_auto_retries_;

  DISALLOW_COPY_AND_ASSIGN(WebstoreDataFetcher);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_H_
