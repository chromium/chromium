// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOMATED_TESTS_CACHE_REPLAYER_H_
#define CHROME_BROWSER_AUTOFILL_AUTOMATED_TESTS_CACHE_REPLAYER_H_

#include <map>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "content/public/test/url_loader_interceptor.h"

namespace autofill {
namespace test {

using ServerCache = std::map<std::string, std::string>;

// Splits raw HTTP request text into a pair, where the first element represent
// the head with headers and the second element represents the body that
// contains the data payload.
std::pair<std::string, std::string> SplitHTTP(const std::string& http_text);

// Streams in text format. For consistency, taken from anonymous namespace in
// components/autofill/core/browser/crowdsourcing/autofill_crowdsourcing_manager.cc
std::ostream& operator<<(std::ostream& out,
                         const autofill::AutofillPageQueryRequest& query);

// Streams in text format. For consistency, taken from anonymous namespace in
// components/autofill/core/browser/form_structure.cc
std::ostream& operator<<(std::ostream& out,
                         const autofill::AutofillQueryResponse& response);
enum class RequestType {
  kQueryProtoGET,
  kQueryProtoPOST,
  kNone,
};

// Gets a key from a given query request.
std::string GetKeyFromQuery(const AutofillPageQueryRequest& query_request);

bool GetResponseForQuery(const ServerCache& cache,
                         const AutofillPageQueryRequest& query,
                         std::string* http_text);

// Switch `--autofill-server-type` is used to override the default behavior of
// using the cached responses from the wpr archive. The valid values match the
// enum AutofillServerBehaviorType below. Options are:
// SavedCache, ProductionServer, or OnlyLocalHeuristics.
constexpr char kAutofillServerBehaviorParam[] = "autofill-server-type";
enum class AutofillServerBehaviorType {
  kSavedCache,          // Uses cached responses. This is the Default.
  kProductionServer,    // Connects to live Autofill Server for recommendations.
  kOnlyLocalHeuristics  // Test with local heuristic recommendations only.
};
// Check for command line flags to determine Autofill Server Behavior type.
AutofillServerBehaviorType ParseAutofillServerBehaviorType();

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
  bool GetApiServerResponseForQuery(const AutofillPageQueryRequest& query,
                                    std::string* http_text) const;

  const ServerCache& cache() const { return cache_; }
  bool split_requests_by_form() const { return split_requests_by_form_; }

 private:
  // Server's cache. Will only be modified during construction of
  // ServerCacheReplayer.
  ServerCache cache_;
  // Controls the type of matching behavior. If False, will cache form signature
  // groupings as they are recorded in the WPR archive. If True, will cache each
  // form individually and attempt to stitch them together on retrieval, which
  // allows a higher hit rate.
  const bool split_requests_by_form_;
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

  // Describes what behavior we want from the autofill server requests
  AutofillServerBehaviorType autofill_server_behavior_type_;

  // Interceptor that intercepts Autofill Server calls.
  content::URLLoaderInterceptor interceptor_;
};

}  // namespace test
}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_AUTOMATED_TESTS_CACHE_REPLAYER_H_
