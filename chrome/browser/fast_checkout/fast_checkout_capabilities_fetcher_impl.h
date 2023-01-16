// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_IMPL_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_IMPL_H_

#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher.h"

#include "components/autofill/core/common/signatures.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/origin.h"

class FastCheckoutCapabilitiesFetcherImpl
    : public FastCheckoutCapabilitiesFetcher {
 public:
  // Possible different cache states that `FastCheckoutCapabilitiesFetcherImpl`
  // can encounter when `IsTriggerFormSupported` is called.
  //
  // Needs to be kept in sync with
  // `FastCheckoutCacheStateForIsTriggerFormSupported` in
  // tools/metrics/histograms/enums.xml.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class CacheStateForIsTriggerFormSupported {
    // Availability is currently being fetched but the request has not completed
    // yet.
    kFetchOngoing = 0,

    // There is a valid cache entry for this origin and the form signature that
    // is being checked is not supported.
    kEntryAvailableAndFormNotSupported = 1,

    // There is a valid cache entry for this origin and the form signature that
    // is being checked is supported.
    kEntryAvailableAndFormSupported = 2,

    // No availability was fetched for this origin within the lifetime of the
    // cache.
    kEntryNotAvailable = 3,

    kMaxValue = kEntryNotAvailable
  };

  // Possible states of parsing the response body when a fetch completes in
  // `FastCheckoutCapabilitiesFetcherImpl`.
  //
  // Needs to be kept in sync with `FastCheckoutCapabilitiesParsingResult` in
  // tools/metrics/histograms/enums.xml.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ParsingResult {
    // The response body was null.
    kNullResponse = 0,

    // The response body could not be parsed as `FastCheckoutFunnels` proto
    // message.
    kParsingError = 1,

    // Parsing was successful.
    kSuccess = 2,

    kMaxValue = kSuccess
  };

  explicit FastCheckoutCapabilitiesFetcherImpl(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~FastCheckoutCapabilitiesFetcherImpl() override;

  FastCheckoutCapabilitiesFetcherImpl(
      const FastCheckoutCapabilitiesFetcherImpl&) = delete;
  FastCheckoutCapabilitiesFetcherImpl& operator=(
      const FastCheckoutCapabilitiesFetcherImpl&) = delete;

  // CapabilitiesFetcher:
  void FetchCapabilities() override;
  bool IsTriggerFormSupported(const url::Origin& origin,
                              autofill::FormSignature form_signature) override;

 private:
  struct FastCheckoutFunnel {
    FastCheckoutFunnel();
    ~FastCheckoutFunnel();
    FastCheckoutFunnel(const FastCheckoutFunnel&);
    base::flat_set<autofill::FormSignature> trigger;
    base::flat_set<autofill::FormSignature> fill;
  };
  // Called when the request's response arrives.
  void OnFetchComplete(base::TimeTicks start_time,
                       std::unique_ptr<std::string> response_body);
  // Returns if the cache is stale, i.e. if `kCacheTimeout` minutes since the
  // last successful request have passed or if no request was done yet.
  bool IsCacheStale() const;
  // URL loader object for the gstatic request. If `url_loader_` is not null, a
  // request is currently ongoing.
  std::unique_ptr<network::SimpleURLLoader> url_loader_ = nullptr;
  // Used for the gstatic requests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  // The cache containing all funnels supported by Fast Checkout. Becomes stale
  // after `kCacheTimeout` minutes.
  base::flat_map<url::Origin, FastCheckoutFunnel> cache_;
  // Last time funnels were fetched successfully.
  base::TimeTicks last_fetch_timestamp_;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_IMPL_H_
