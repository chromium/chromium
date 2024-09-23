// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_ESSENTIAL_SEARCH_SOCS_COOKIE_FETCHER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_ESSENTIAL_SEARCH_SOCS_COOKIE_FETCHER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace app_list {

// This class is responsible for fetching SOCS cookie from chromeoscompliance
// API and pass it to its consumer after the request is completed.
// SocsCookieFetcher is still WIP.
class SocsCookieFetcher final {
 public:
  // This enum is tied directly to a UMA enum defined in
  // //tools/metrics/histograms/metadata/ash/enums.xml, and should
  // always reflect it (do not change one without changing the other).
  // These values are persisted to logs. Entries should not be renumbered
  // and numeric values should never be reused.
  enum class Status {
    kOk = 0,
    kRequestBodyNotSerialized = 1,
    kServerError = 2,
    kEmptyResponse = 3,
    kJsonParseFailure = 4,
    kNotJsonDict = 5,
    kFetchNoCookie = 6,
    kInvalidCookie = 7,
    kCookieInsertionFailure = 8,
    kMaxValue = kCookieInsertionFailure
  };

  class Consumer {
   public:
    Consumer();
    virtual ~Consumer();
    virtual void OnCookieFetched(const std::string& cookie_header) = 0;
    virtual void OnApiCallFailed(Status status) = 0;
  };

  SocsCookieFetcher(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      Consumer* consumer);
  ~SocsCookieFetcher();

  // Disallow copy and assign.
  SocsCookieFetcher(const SocsCookieFetcher&) = delete;
  SocsCookieFetcher& operator=(const SocsCookieFetcher&) = delete;

  void StartFetching();

 private:
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);
  void OnJsonParsed(data_decoder::DataDecoder::ValueOrError result);
  void ProcessValidTokenResponse(base::Value::Dict json_response);

  // `consumer_` to call back when this request completes.
  const raw_ptr<Consumer> consumer_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<network::SimpleURLLoader> simple_url_loader_;

  base::WeakPtrFactory<SocsCookieFetcher> weak_ptr_factory_{this};
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_ESSENTIAL_SEARCH_SOCS_COOKIE_FETCHER_H_
