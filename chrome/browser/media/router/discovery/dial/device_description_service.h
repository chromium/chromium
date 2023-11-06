// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DEVICE_DESCRIPTION_SERVICE_H_
#define CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DEVICE_DESCRIPTION_SERVICE_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/media/router/discovery/dial/dial_device_data.h"
#include "chrome/browser/media/router/discovery/dial/parsed_dial_device_description.h"
#include "chrome/browser/media/router/discovery/dial/safe_dial_device_description_parser.h"

namespace media_router {

class DeviceDescriptionFetcher;
class SafeDialDeviceDescriptionParser;

// This class fetches and parses device description XML for DIAL devices. Actual
// parsing happens in a separate utility process via SafeDeviceDescriptionParser
// (instead of in this class).
// This class is not sequence safe.
class DeviceDescriptionService {
 public:
  // Represents cached device description data parsed from device description
  // XML.
  struct CacheEntry {
    CacheEntry();
    CacheEntry(const CacheEntry& other);
    ~CacheEntry();

    // The expiration time from the cache.
    base::Time expire_time;

    // The device description version number (non-negative).
    int32_t config_id;

    // Parsed device description data from XML.
    ParsedDialDeviceDescription description_data;
  };

  // Called if parsing device description XML in utility process succeeds, and
  // all fields are valid.
  // |device_data|: The device to look up.
  // |description_data|: Device description data from device description XML.
  using DeviceDescriptionParseSuccessCallback = base::RepeatingCallback<void(
      const DialDeviceData& device_data,
      const ParsedDialDeviceDescription& description_data)>;

  // Called if parsing device description XML in utility process fails, or some
  // parsed fields are missing or invalid.
  using DeviceDescriptionParseErrorCallback =
      base::RepeatingCallback<void(const DialDeviceData& device_data,
                                   const std::string& error_message)>;

  DeviceDescriptionService(
      const DeviceDescriptionParseSuccessCallback& success_cb,
      const DeviceDescriptionParseErrorCallback& error_cb);

  DeviceDescriptionService(const DeviceDescriptionService&) = delete;
  DeviceDescriptionService& operator=(const DeviceDescriptionService&) = delete;

  virtual ~DeviceDescriptionService();

  // For each device in |devices|, if there is a valid cache entry for it, call
  // |success_cb_| with cached device description; otherwise start fetching
  // device description XML and parsing XML in utility process. Call
  // |success_cb_| if both fetching and parsing succeeds; otherwise call
  // |error_cb_|.
  virtual void GetDeviceDescriptions(
      const std::vector<DialDeviceData>& devices);

 protected:
  // Parses the device description data in |description_data| and invokes
  // OnParsedDeviceDescription() when done, passing |device_data| along.
  // Made visible so it can be overwritten in tests.
  virtual void ParseDeviceDescription(
      const DialDeviceData& device_data,
      const DialDeviceDescriptionData& description_data);

  // Overridden by unit tests.
  virtual std::unique_ptr<DeviceDescriptionFetcher> CreateFetcher(
      const DialDeviceData& device_data,
      base::OnceCallback<void(const DialDeviceDescriptionData&)> success_cb,
      base::OnceCallback<void(const std::string&)> error_cb);

 private:
  friend class DeviceDescriptionServiceTest;
  friend class TestDeviceDescriptionService;
  FRIEND_TEST_ALL_PREFIXES(DeviceDescriptionServiceTest,
                           TestGetDeviceDescriptionRemoveOutDatedFetchers);
  FRIEND_TEST_ALL_PREFIXES(DeviceDescriptionServiceTest,
                           TestGetDeviceDescriptionFetchURL);
  FRIEND_TEST_ALL_PREFIXES(DeviceDescriptionServiceTest,
                           TestGetDeviceDescriptionFetchURLError);
  FRIEND_TEST_ALL_PREFIXES(DeviceDescriptionServiceTest,
                           TestCleanUpCacheEntries);
  FRIEND_TEST_ALL_PREFIXES(DeviceDescriptionServiceTest,
                           TestSafeParserProperlyCreated);

  // Checks the cache for a valid device description. If the entry is found but
  // is expired, it is removed from the cache. Returns cached entry of
  // parsed device description. Returns nullptr if cache entry does not exist or
  // is not valid.
  // |device_data|: The device to look up.
  const CacheEntry* CheckAndUpdateCache(const DialDeviceData& device_data);

  // Issues a HTTP GET request for the device description. No-op if there is
  // already a pending request.
  // |device_data|: The device to look up.
  void FetchDeviceDescription(const DialDeviceData& device_data);

  // Invoked when HTTP GET request finishes.
  // |device_data|: Device data initiating the HTTP request.
  // |description_data|: Response from HTTP request.
  void OnDeviceDescriptionFetchComplete(
      const DialDeviceData& device_data,
      const DialDeviceDescriptionData& description_data);

  // Invoked when HTTP GET request fails.
  // |device_data|: Device data initiating the HTTP request.
  // |error_message|: Error message from HTTP request.
  void OnDeviceDescriptionFetchError(const DialDeviceData& device_data,
                                     const std::string& error_message);

  // Invoked when SafeDialDeviceDescriptionParser finishes parsing device
  // description XML.
  // |device_data|: Device data initiating XML parsing in utility process.
  // |device_description|: Parsed device description from utility process,
  // empty if parsing failed.
  // |parsing_error|: error encountered while parsing DIAL device description.
  void OnParsedDeviceDescription(
      const DialDeviceData& device_data,
      const ParsedDialDeviceDescription& device_description,
      SafeDialDeviceDescriptionParser::ParsingResult parsing_result);

  // Remove expired cache entries from |description_map_|.
  void CleanUpCacheEntries();

  // Used by unit tests.
  virtual base::Time GetNow();

  // Map of current device description fetches in progress, keyed by device
  // label.
  std::map<std::string, std::unique_ptr<DeviceDescriptionFetcher>>
      device_description_fetcher_map_;

  // The number of device description still pending to be parsed.
  uint32_t pending_device_count_ = 0U;

  // Map of current cached device descriptions, keyed by device label.
  std::map<std::string, CacheEntry> description_cache_;

  // See comments for DeviceDescriptionParseSuccessCallback.
  DeviceDescriptionParseSuccessCallback success_cb_;

  // See comments for DeviceDescriptionParseErrorCallback.
  DeviceDescriptionParseErrorCallback error_cb_;

  // Timer for clean up expired cache entries.
  std::unique_ptr<base::RepeatingTimer> clean_up_timer_;

  // Safe DIAL parser. Does the parsing in a utility process.
  SafeDialDeviceDescriptionParser device_description_parser_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_DISCOVERY_DIAL_DEVICE_DESCRIPTION_SERVICE_H_
