// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_APP_DISCOVERY_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_APP_DISCOVERY_SERVICE_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/media/router/discovery/dial/dial_url_fetcher.h"
#include "chrome/browser/media/router/discovery/dial/parsed_dial_app_info.h"
#include "chrome/browser/media/router/discovery/dial/safe_dial_app_info_parser.h"
#include "chrome/browser/media/router/logger_list.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "url/gurl.h"

namespace media_router {

// Represents DIAL app status on receiver device.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DialAppInfoResultCode {
  kOk = 0,
  // kNotFound = 1, no longer used. Do not reuse the value 1.
  kNetworkError = 2,
  kParsingError = 3,
  kHttpError = 4,
  kCount
};

struct DialAppInfoResult {
  DialAppInfoResult(std::unique_ptr<ParsedDialAppInfo> app_info,
                    DialAppInfoResultCode result_code,
                    const std::string& error_message = "",
                    std::optional<int> http_error_code = std::nullopt);
  DialAppInfoResult(DialAppInfoResult&& other);
  ~DialAppInfoResult();

  // Parsed app info on the device for the given app, or nullptr if unable to
  // fetch/parse app info.
  std::unique_ptr<ParsedDialAppInfo> app_info;
  // |kOk| on success, a failure code otherwise.
  DialAppInfoResultCode result_code;
  // Optionally set to provide additional information for an error.
  std::string error_message;
  // Set when |result_code| is |kHttpError|.
  std::optional<int> http_error_code;
};

// This class provides an API to fetch DIAL app info XML from an app URL and
// parse the XML into a DialAppInfo object. Actual parsing happens in a
// separate utility process via SafeDialAppInfoParser instead of in this class.
// During shutdown, this class aborts all pending requests and no callbacks get
// invoked.
// This class is not sequence safe.
class DialAppDiscoveryService {
 public:
  // |sink_id|: MediaSink ID of the receiver that responded to the GET request.
  // |app_name|: DIAL app name whose status is being checked on |sink_id|.
  // |result|: Result of the app info fetching/parsing.
  using DialAppInfoCallback =
      base::OnceCallback<void(const MediaSink::Id& sink_id,
                              const std::string& app_name,
                              DialAppInfoResult result)>;

  DialAppDiscoveryService();

  DialAppDiscoveryService(const DialAppDiscoveryService&) = delete;
  DialAppDiscoveryService& operator=(const DialAppDiscoveryService&) = delete;

  virtual ~DialAppDiscoveryService();

  // Queries |app_name|'s availability on |sink| by issuing a HTTP GET request.
  // App URL is used to issue HTTP GET request. E.g.
  // http://127.0.0.1/apps/YouTube. "http://127.0.0.1/apps/" is the base part
  // which comes from |sink|; "YouTube" suffix is the app name part which comes
  // from |app_name|.
  virtual void FetchDialAppInfo(const MediaSinkInternal& sink,
                                const std::string& app_name,
                                DialAppInfoCallback app_info_cb);

 private:
  friend class DialAppDiscoveryServiceTest;

  class PendingRequest {
   public:
    PendingRequest(const MediaSinkInternal& sink,
                   const std::string& app_name,
                   DialAppInfoCallback app_info_cb,
                   DialAppDiscoveryService* const service);

    PendingRequest(const PendingRequest&) = delete;
    PendingRequest& operator=(const PendingRequest&) = delete;

    ~PendingRequest();

    // Starts fetching the app info on |app_url_|.
    void Start();

   private:
    friend class DialAppDiscoveryServiceTest;

    // Invoked when HTTP GET request finishes.
    // |app_info_xml|: Response XML from HTTP request.
    void OnDialAppInfoFetchComplete(const std::string& app_info_xml);

    // Invoked when HTTP GET request fails.
    void OnDialAppInfoFetchError(const std::string& error_message,
                                 std::optional<int> http_response_code);

    // Invoked when SafeDialAppInfoParser finishes parsing app info XML.
    // |app_info|: Parsed app info from utility process, or nullptr if parsing
    // failed.
    // |parsing_result|: Result of DIAL app info XML parsing.
    void OnDialAppInfoParsed(
        std::unique_ptr<ParsedDialAppInfo> app_info,
        SafeDialAppInfoParser::ParsingResult parsing_result);

    MediaSink::Id sink_id_;
    std::string app_name_;
    GURL app_url_;
    DialURLFetcher fetcher_;
    DialAppInfoCallback app_info_cb_;

    // Raw pointer to DialAppDiscoveryService that owns |this|.
    const raw_ptr<DialAppDiscoveryService> service_;

    SEQUENCE_CHECKER(sequence_checker_);
    base::WeakPtrFactory<PendingRequest> weak_ptr_factory_{this};
  };

  friend class PendingRequest;

  // Used by unit test.
  void SetParserForTest(std::unique_ptr<SafeDialAppInfoParser> parser);

  // Called by PendingRequest to delete itself.
  void RemovePendingRequest(PendingRequest* request);

  // Pending app info requests.
  std::vector<std::unique_ptr<PendingRequest>> pending_requests_;

  // Safe DIAL parser. Does the parsing in a utility process.
  std::unique_ptr<SafeDialAppInfoParser> parser_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DIAL_APP_DISCOVERY_SERVICE_H_
