// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_FETCHER_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_FETCHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/android/explore_sites/explore_sites_types.h"
#include "net/base/backoff_entry.h"

namespace network {
class SimpleURLLoader;
class SharedURLLoaderFactory;
}  // namespace network

namespace explore_sites {

// A class that fetches data from the server.
class ExploreSitesFetcher {
 public:
  // Callback to pass back the catalog returned from the server.
  // Invoked with |nullptr| if there is an error.
  using Callback =
      base::OnceCallback<void(ExploreSitesRequestStatus status,
                              const std::unique_ptr<std::string> data)>;

  static const net::BackoffEntry::Policy kImmediateFetchBackoffPolicy;
  static const int kMaxFailureCountForImmediateFetch;
  static const net::BackoffEntry::Policy kBackgroundFetchBackoffPolicy;
  static const int kMaxFailureCountForBackgroundFetch;

  // Creates a fetcher for the GetCatalog RPC.
  static std::unique_ptr<ExploreSitesFetcher> CreateForGetCatalog(
      bool is_immediate_fetch,
      const std::string& catalog_version,
      const std::string& accept_languages,
      const std::string& country_code,
      scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
      Callback callback);

  ~ExploreSitesFetcher();

  // Starts the fetching.
  void Start();

  // Restarts as immediate fetching if background fetching is in progress.
  // Nothing is done if already doing immediate fetching.
  void RestartAsImmediateFetchIfNotYet();

  bool is_immediate_fetch() const { return is_immediate_fetch_; }
  void disable_retry_for_testing() { disable_retry_for_testing_ = true; }

  // Delegate that knows how to get data from the device.  Can be overridden for
  // testing.
  class DeviceDelegate {
   public:
    DeviceDelegate() = default;
    virtual ~DeviceDelegate() = default;
    virtual float GetScaleFactorFromDevice();
  };

  // Allow overriding device specific functionality for testing
  void SetDeviceDelegateForTest(std::unique_ptr<DeviceDelegate> delegate);
  void OverrideCountryCodeForDebugging(const std::string& country_code);

 private:
  explicit ExploreSitesFetcher(
      bool is_immediate_fetch,
      const GURL& url,
      const std::string& catalog_version,
      const std::string& accept_languages,
      const std::string& country_code,
      scoped_refptr<network ::SharedURLLoaderFactory> loader_factory,
      Callback callback);

  // Invoked from SimpleURLLoader after download is complete.
  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body);

  ExploreSitesRequestStatus HandleResponseCode();

  void RetryWithBackoff();

  // Updates the backoff values based on current fetch mode.
  void UpdateBackoffEntry();

  bool is_immediate_fetch_;
  std::string accept_languages_;
  std::string country_code_;
  std::string client_version_;
  std::string catalog_version_;
  GURL url_;

  std::unique_ptr<net::BackoffEntry> backoff_entry_;
  int max_failure_count_ = 0;
  bool disable_retry_for_testing_ = false;

  std::unique_ptr<DeviceDelegate> device_delegate_;
  Callback callback_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  std::unique_ptr<network::SimpleURLLoader> url_loader_;

  base::WeakPtrFactory<ExploreSitesFetcher> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesFetcher);
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_FETCHER_H_
