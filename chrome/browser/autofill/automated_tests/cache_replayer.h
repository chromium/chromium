// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOMATED_TESTS_CACHE_REPLAYER_H_
#define CHROME_BROWSER_AUTOFILL_AUTOMATED_TESTS_CACHE_REPLAYER_H_

#include <map>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "content/public/test/url_loader_interceptor.h"

namespace autofill {
namespace test {

using ServerCache = std::map<std::string, std::string>;

// Splits raw HTTP request text into a pair, where the first element represent
// the head with headers and the second element represents the body that
// contains the data payload.
std::pair<std::string, std::string> SplitHTTP(const std::string& http_text);

// Streams in text format. For consistency, taken from anonymous namespace in
// components/autofill/core/browser/autofill_download_manager.cc
std::ostream& operator<<(std::ostream& out,
                         const autofill::AutofillQueryContents& query);

// Streams in text format. For consistency, taken from anonymous namespace in
// components/autofill/core/browser/form_structure.cc
std::ostream& operator<<(
    std::ostream& out,
    const autofill::AutofillQueryResponseContents& response);

// Gets a key from a given query request.
std::string GetKeyFromQueryRequest(const AutofillQueryContents& query_request);

enum class RequestType { kLegacyQueryProtoGET, kLegacyQueryProtoPOST };

// Replayer for Autofill Server cache. Can be used in concurrency.
class ServerCacheReplayer {
 public:
  enum Options {
    kOptionNone = 0,
    kOptionFailOnInvalidJsonRecord = 1 << 1,
    kOptionFailOnEmpty = 1 << 2,
    kOptionSplitRequestsByForm = 1 << 3,
  };

  // Container for status.
  enum class StatusCode { kOk = 0, kEmpty = 1, kBadRead = 2, kBadNode = 3 };
  struct Status {
    StatusCode error_code;
    std::string message;

    bool Ok() { return error_code == StatusCode::kOk; }
  };

  // Populates the cache at construction time. File at |json_file_path| has to
  // contain a textual json structured like this (same as WPR):
  // {
  //   'Requests': {
  //      'clients1.google.com': {
  //        'https://clients1.google.com/tbproxy/af/query?': [
  //          {'SerializedRequest': '1234', 'SerializedResponse': '1234'}
  //        ]
  //      }
  //   }
  // }
  // You can set the replayer's behavior by setting |options| with a mix of
  // Options. Will crash if there is a failure when loading the cache.
  ServerCacheReplayer(const base::FilePath& json_file_path, int options);
  // Constructs the replayer from an already populated cache.
  explicit ServerCacheReplayer(ServerCache server_cache,
                               bool split_requests_by_form = false);
  ~ServerCacheReplayer();

  // Gets an uncompress HTTP textual response that is paired with |query| as
  // key. Returns false if there was no match or the response could no be
  // decompressed. Nothing will be assigned to |http_text| on error. Leaves a
  // log when there is an error. Can be used in concurrency.
  bool GetResponseForQuery(const AutofillQueryContents& query,
                           std::string* http_text) const;

 private:
  // Server's cache. Will only be modified during construction of
  // ServerCacheReplayer.
  ServerCache cache_;
  // Represents the cache at read time.
  const ServerCache& const_cache_ = cache_;
  // Controls the type of matching behavior. If False, will cache form signature
  // groupings as they are recorded in the WPR archive. If True, will cache each
  // form individually and attempt to stitch them together on retrieval, which
  // allows a higher hit rate.
  const bool split_requests_by_form_;

  // Assumes key exists, and decompresses and returns http body which is stored
  // at that location.
  bool RetrieveAndDecompressStoredHTTP(const std::string& key,
                                       std::string* decompressed_http) const;
};

// Url loader that intercepts Autofill Server calls and serves back cached
// content.
class ServerUrlLoader {
 public:
  explicit ServerUrlLoader(std::unique_ptr<ServerCacheReplayer> cache_replayer);
  ~ServerUrlLoader();

 private:
  bool InterceptAutofillRequest(
      content::URLLoaderInterceptor::RequestParams* params);

  // Cache replayer component that is used to replay cached responses.
  std::unique_ptr<ServerCacheReplayer> cache_replayer_;

  // Interceptor that intercepts Autofill Server calls.
  content::URLLoaderInterceptor interceptor_;
};

}  // namespace test
}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOMATED_TESTS_CACHE_REPLAYER_H_
