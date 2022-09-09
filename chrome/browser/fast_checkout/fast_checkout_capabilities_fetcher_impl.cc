// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "components/keyed_service/core/keyed_service.h"
#include "net/http/http_status_code.h"
#include "url/gurl.h"
#include "url/origin.h"

using autofill_assistant::AutofillAssistant;
using CapabilitiesInfo =
    autofill_assistant::AutofillAssistant::CapabilitiesInfo;

namespace {

constexpr uint32_t kFastCheckoutHashPrefixSize = 10u;
constexpr char kFastCheckoutIntent[] = "CHROME_FAST_CHECKOUT";
constexpr char kUmaKeyCacheStateIsTriggerFormSupported[] =
    "Autofill.FastCheckout.CapabilitiesFetcher."
    "CacheStateForIsTriggerFormSupported";
constexpr char kUmaKeyHttpCode[] =
    "Autofill.FastCheckout.CapabilitiesFetcher.HttpResponseCode";
constexpr char kUmaKeyResponseTime[] =
    "Autofill.FastCheckout.CapabilitiesFetcher.ResponseTime";

}  // namespace

FastCheckoutCapabilitiesFetcherImpl::FastCheckoutCapabilitiesFetcherImpl(
    std::unique_ptr<AutofillAssistant> autofill_assistant)
    : autofill_assistant_(std::move(autofill_assistant)) {}

FastCheckoutCapabilitiesFetcherImpl::~FastCheckoutCapabilitiesFetcherImpl() =
    default;

void FastCheckoutCapabilitiesFetcherImpl::FetchAvailability(
    const url::Origin& origin,
    Callback callback) {
  // If `origin` is already cached, no request needs to be made.
  if (cache_.ContainsOrigin(origin)) {
    std::move(callback).Run(/*success=*/true);
    return;
  }

  // Check whether there is an ongoing request. If so, queue up the callback
  // and return.
  if (RequestMap::iterator it = ongoing_requests_.find(origin);
      it != ongoing_requests_.end()) {
    it->second.emplace_back(std::move(callback));
    return;
  }

  // Create a new request.
  uint64_t hash_prefix =
      AutofillAssistant::GetHashPrefix(kFastCheckoutHashPrefixSize, origin);
  ongoing_requests_[origin].emplace_back(std::move(callback));
  // Since `this` owns `autofill_assistant_` and `autofill_assistant_`,
  // callbacks are only executed while `this` is alive.
  autofill_assistant_->GetCapabilitiesByHashPrefix(
      kFastCheckoutHashPrefixSize, {hash_prefix}, kFastCheckoutIntent,
      base::BindOnce(&FastCheckoutCapabilitiesFetcherImpl::
                         OnGetCapabilitiesInformationReceived,
                     base::Unretained(this), origin, base::TimeTicks::Now()));
}

bool FastCheckoutCapabilitiesFetcherImpl::IsTriggerFormSupported(
    const url::Origin& origin,
    autofill::FormSignature form_signature) {
  if (base::FeatureList::IsEnabled(
          features::kForceEnableFastCheckoutCapabilities)) {
    return true;
  }
  if (cache_.ContainsTriggerForm(origin, form_signature)) {
    base::UmaHistogramEnumeration(
        kUmaKeyCacheStateIsTriggerFormSupported,
        CacheStateForIsTriggerFormSupported::kEntryAvailableAndFormSupported);
    return true;
  }

  // Analyze why the result is `false` to record the correct metric.
  if (cache_.ContainsOrigin(origin)) {
    base::UmaHistogramEnumeration(kUmaKeyCacheStateIsTriggerFormSupported,
                                  CacheStateForIsTriggerFormSupported::
                                      kEntryAvailableAndFormNotSupported);
  } else {
    base::UmaHistogramEnumeration(
        kUmaKeyCacheStateIsTriggerFormSupported,
        ongoing_requests_.contains(origin)
            ? CacheStateForIsTriggerFormSupported::kFetchOngoing
            : CacheStateForIsTriggerFormSupported::kNeverFetched);
  }
  return false;
}

void FastCheckoutCapabilitiesFetcherImpl::OnGetCapabilitiesInformationReceived(
    const url::Origin& origin,
    base::TimeTicks start_time,
    int http_status,
    const std::vector<CapabilitiesInfo>& capabilities) {
  RequestMap::iterator request = ongoing_requests_.find(origin);
  if (request == ongoing_requests_.end()) {
    // There should always be exactly one ongoing request per origin.
    NOTREACHED();
    return;
  }

  base::UmaHistogramSparse(kUmaKeyHttpCode, http_status);
  base::UmaHistogramMediumTimes(kUmaKeyResponseTime,
                                base::TimeTicks::Now() - start_time);

  // Short-hand for executing all callbacks.
  auto inform_callers = [request](bool outcome) {
    for (Callback& callback : request->second) {
      std::move(callback).Run(outcome);
    }
  };

  // If the request was unsuccessful, inform the callers, but do not update
  // the cache.
  if (http_status != net::HTTP_OK) {
    inform_callers(false);
    ongoing_requests_.erase(request);
    return;
  }

  std::vector<CapabilitiesInfo>::const_iterator request_capabilities =
      base::ranges::find_if(
          capabilities, [&origin](const CapabilitiesInfo& info) {
            return url::Origin::Create(GURL(info.url)) == origin;
          });

  if (request_capabilities != capabilities.end() &&
      request_capabilities->bundle_capabilities_information.has_value()) {
    cache_.AddToCache(
        origin,
        FastCheckoutCapabilitiesResult(
            request_capabilities->bundle_capabilities_information.value()
                .trigger_form_signatures));
  } else {
    // If no form signatures are supported, save that into the cache, too.
    cache_.AddToCache(origin, FastCheckoutCapabilitiesResult());
  }

  inform_callers(true);
  ongoing_requests_.erase(request);
}
