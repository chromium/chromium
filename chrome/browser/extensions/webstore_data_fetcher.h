// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_H_
#define CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/cws_item_service.pb.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

namespace network::mojom {
class URLLoaderFactory;
class URLResponseHead;
}  // namespace network::mojom

namespace extensions {

class WebstoreDataFetcherDelegate;

// WebstoreDataFetcher fetches web store data and parses it into a
// DictionaryValue.
class WebstoreDataFetcher {
 public:
  WebstoreDataFetcher(WebstoreDataFetcherDelegate* delegate,
                      const GURL& referrer_url,
                      const std::string& webstore_item_id);

  WebstoreDataFetcher(const WebstoreDataFetcher&) = delete;
  WebstoreDataFetcher& operator=(const WebstoreDataFetcher&) = delete;

  ~WebstoreDataFetcher();

  static void SetLogResponseCodeForTesting(bool enabled);

  // Sets a mock response that is returned by the item snippets API when
  // FetchItemSnippet is called. `mock_response` is owned by the test that calls
  // this method.
  static void SetMockItemSnippetReponseForTesting(
      FetchItemSnippetResponse* mock_response);

  void Start(network::mojom::URLLoaderFactory* url_loader_factory);

  void set_max_auto_retries(int max_retries) {
    max_auto_retries_ = max_retries;
  }

 private:
  // Fetch web store data using the item JSON API.
  // TODO(kelvinjiang): Remove this and all related methods in
  // WebstoreDataFetcherDelegate after migration to the new item snippet API is
  // complete.
  void FetchFromItemJSONAPI(
      network::mojom::URLLoaderFactory* url_loader_factory);

  // Fetch web store data using the new item snippets API.
  void FetchItemSnippet(network::mojom::URLLoaderFactory* url_loader_factory);

  // Initializes `simple_url_loader_` for the given `request` and `annotation`.
  void InitializeSimpleLoaderForRequest(
      std::unique_ptr<network::ResourceRequest> request,
      const net::NetworkTrafficAnnotationTag& annotation);

  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError result);
  void OnResponseStarted(const GURL& final_url,
                         const network::mojom::URLResponseHead& response_head);

  // Called when a response is received from the item JSON API.
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  // Called when a response is received from the new item snippet API.
  void OnFetchItemSnippetResponseReceived(
      std::unique_ptr<std::string> response_body);

  raw_ptr<WebstoreDataFetcherDelegate> delegate_;
  GURL referrer_url_;
  std::string id_;
  std::string post_data_;

  // For fetching webstore JSON data.
  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  // Maximum auto retry times on server 5xx error or ERR_NETWORK_CHANGED.
  // Default is 0 which means to use the URLFetcher default behavior.
  int max_auto_retries_ = 0;

  base::WeakPtrFactory<WebstoreDataFetcher> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WEBSTORE_DATA_FETCHER_H_
