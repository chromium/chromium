// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_IMPL_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_IMPL_H_

#include <memory>
#include <utility>

#include "base/callback_forward.h"
#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_results_cache.h"
#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

namespace autofill {
class FormSignature;
}  // namespace autofill

class FastCheckoutCapabilitiesFetcherImpl
    : public FastCheckoutCapabilitiesFetcher {
 public:
  // Possible different cache states that `FastCheckoutCapabilitiesFetcherImpl`
  // can encounter when `IsTriggerFormSupported` is called.
  //
  // Do not remove or renumber entries in this enum. It needs to be kept in
  // sync with `FastCheckoutCacheStateForIsTriggerFormSupported` in `enums.xml`.
  enum class CacheStateForIsTriggerFormSupported {
    // Availability is currently being fetched for this entry, but the request
    // has not completed yet.
    kFetchOngoing = 0,

    // There is a valid cache entry for this origin and the form signature that
    // is being checked is not supported.
    kEntryAvailableAndFormNotSupported = 1,

    // There is a valid cache entry for this origin and the form signature that
    // is being checked is supported.
    kEntryAvailableAndFormSupported = 2,

    // No availability was fetched for this origin within the lifetime of the
    // cache.
    kNeverFetched = 3,

    kMaxValue = kNeverFetched
  };

  explicit FastCheckoutCapabilitiesFetcherImpl(
      std::unique_ptr<autofill_assistant::AutofillAssistant>
          autofill_assistant);
  ~FastCheckoutCapabilitiesFetcherImpl() override;

  FastCheckoutCapabilitiesFetcherImpl(
      const FastCheckoutCapabilitiesFetcherImpl&) = delete;
  FastCheckoutCapabilitiesFetcherImpl& operator=(
      const FastCheckoutCapabilitiesFetcherImpl&) = delete;

  // CapabilitiesFetcher:
  void FetchAvailability(const url::Origin& origin, Callback callback) override;
  bool IsTriggerFormSupported(const url::Origin& origin,
                              autofill::FormSignature form_signature) override;

 private:
  // Abbreviation for a map of origins to a pair of the start time (in time
  // ticks) and a vector of callbacks.
  using RequestMap = base::flat_map<url::Origin, std::vector<Callback>>;

  // Processes the result returned by from a previous
  // `AutofillAssistant::GetCapabilitiesByHashPrefix` call and informs callers
  // that availability has been fetched.
  void OnGetCapabilitiesInformationReceived(
      const url::Origin& origin,
      base::TimeTicks start_time,
      int http_status,
      const std::vector<
          autofill_assistant::AutofillAssistant::CapabilitiesInfo>&
          capabilities);

  // An `AutofillAssistant` instance to gain access to
  // `GetCapabilitiesByHashPrefix` RPC calls.
  const std::unique_ptr<autofill_assistant::AutofillAssistant>
      autofill_assistant_;

  // The cache of known capabilities results.
  FastCheckoutCapabilitiesResultsCache cache_;

  // Currently ongoing capabilities requests.
  RequestMap ongoing_requests_;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_IMPL_H_
