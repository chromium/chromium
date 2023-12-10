// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_impl.h"

#include <memory>

#include "base/metrics/histogram_functions.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "url/origin.h"

namespace {
constexpr int kMaxDownloadSizeInBytes = 10 * 1024;
constexpr char kFastCheckoutFunnelsUrl[] =
    "https://www.gstatic.com/autofill/fast_checkout/funnels.binarypb";
constexpr base::TimeDelta kCacheTimeout(base::Minutes(10));
constexpr base::TimeDelta kFetchTimeout(base::Seconds(3));
constexpr char kUmaKeyCacheStateIsTriggerFormSupported[] =
    "Autofill.FastCheckout.CapabilitiesFetcher."
    "CacheStateForIsTriggerFormSupported";
constexpr char kUmaKeyParsingResult[] =
    "Autofill.FastCheckout.CapabilitiesFetcher.ParsingResult";
constexpr char kUmaKeyResponseAndNetErrorCode[] =
    "Autofill.FastCheckout.CapabilitiesFetcher.HttpResponseAndNetErrorCode";
constexpr char kUmaKeyResponseTime[] =
    "Autofill.FastCheckout.CapabilitiesFetcher.ResponseTime";
}  // namespace

FastCheckoutCapabilitiesFetcherImpl::FastCheckoutFunnel::FastCheckoutFunnel() =
    default;

FastCheckoutCapabilitiesFetcherImpl::FastCheckoutFunnel::~FastCheckoutFunnel() =
    default;

FastCheckoutCapabilitiesFetcherImpl::FastCheckoutFunnel::FastCheckoutFunnel(
    const FastCheckoutFunnel&) = default;

FastCheckoutCapabilitiesFetcherImpl::FastCheckoutCapabilitiesFetcherImpl(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : url_loader_factory_(url_loader_factory) {}

FastCheckoutCapabilitiesFetcherImpl::~FastCheckoutCapabilitiesFetcherImpl() =
    default;

void FastCheckoutCapabilitiesFetcherImpl::FetchCapabilities() {
  if (url_loader_) {
    // There is an ongoing request.
    return;
  }
  if (!IsCacheStale()) {
    return;
  }
  cache_.clear();
  auto resource_request = std::make_unique<network::ResourceRequest>();
  resource_request->url = GURL(kFastCheckoutFunnelsUrl);
  resource_request->credentials_mode = network::mojom::CredentialsMode::kOmit;
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("gstatic_fast_checkout_funnels",
                                          R"(
        semantics {
          sender: "Fast Checkout Tab Helper"
          description:
            "A binary proto string containing all funnels supported by Fast "
            "Checkout."
          trigger:
            "When the user visits a checkout page."
          data:
            "The request body is empty. No user data is included."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "The user can enable or disable this feature via 'Save and fill "
            "payment methods' and 'Save and fill addresses' in Chromium's "
            "settings under 'Payment methods' and 'Addresses and more' "
            "respectively. The feature is enabled by default."
          chrome_policy {
            AutofillCreditCardEnabled {
                policy_options {mode: MANDATORY}
                AutofillCreditCardEnabled: true
            }
          }
          chrome_policy {
            AutofillAddressEnabled {
                policy_options {mode: MANDATORY}
                AutofillAddressEnabled: true
            }
          }
        })");
  url_loader_ = network::SimpleURLLoader::Create(std::move(resource_request),
                                                 traffic_annotation);
  url_loader_->SetTimeoutDuration(kFetchTimeout);
  url_loader_->DownloadToString(
      url_loader_factory_.get(),
      base::BindOnce(&FastCheckoutCapabilitiesFetcherImpl::OnFetchComplete,
                     base::Unretained(this), base::TimeTicks::Now()),
      kMaxDownloadSizeInBytes);
}

bool FastCheckoutCapabilitiesFetcherImpl::IsCacheStale() const {
  return last_fetch_timestamp_.is_null() ||
         base::TimeTicks::Now() - last_fetch_timestamp_ >= kCacheTimeout;
}

void FastCheckoutCapabilitiesFetcherImpl::OnFetchComplete(
    base::TimeTicks start_time,
    std::unique_ptr<std::string> response_body) {
  base::UmaHistogramTimes(kUmaKeyResponseTime,
                          base::TimeTicks::Now() - start_time);

  int net_error = url_loader_->NetError();
  bool report_http_response_code =
      (net_error == net::OK ||
       net_error == net::ERR_HTTP_RESPONSE_CODE_FAILURE) &&
      url_loader_->ResponseInfo() && url_loader_->ResponseInfo()->headers;
  base::UmaHistogramSparse(
      kUmaKeyResponseAndNetErrorCode,
      report_http_response_code
          ? url_loader_->ResponseInfo()->headers->response_code()
          : net_error);

  // Reset `url_loader_` so that another request could be made.
  url_loader_.reset();
  last_fetch_timestamp_ = base::TimeTicks::Now();

  if (net_error != net::OK) {
    return;
  }

  if (!response_body) {
    base::UmaHistogramEnumeration(kUmaKeyParsingResult,
                                  ParsingResult::kNullResponse);
    return;
  }

  ::fast_checkout::FastCheckoutFunnels funnels;
  if (!funnels.ParseFromString(*response_body)) {
    base::UmaHistogramEnumeration(kUmaKeyParsingResult,
                                  ParsingResult::kParsingError);
    return;
  }

  base::UmaHistogramEnumeration(kUmaKeyParsingResult, ParsingResult::kSuccess);

  for (const ::fast_checkout::FastCheckoutFunnels_FastCheckoutFunnel&
           funnel_proto : funnels.funnels()) {
    AddFunnelToCache(funnel_proto);
  }
}

void FastCheckoutCapabilitiesFetcherImpl::AddFunnelToCache(
    const ::fast_checkout::FastCheckoutFunnels_FastCheckoutFunnel&
        funnel_proto) {
  // There has to be at least one trigger form signature for a funnel. Otherwise
  // a run could never be triggered successfully.
  if (funnel_proto.trigger().empty()) {
    return;
  }

  FastCheckoutFunnel funnel;
  for (uint64_t form_signature : funnel_proto.trigger()) {
    funnel.trigger.emplace(form_signature);
  }
  for (uint64_t form_signature : funnel_proto.fill()) {
    funnel.fill.emplace(form_signature);
  }

  for (const std::string& domain : funnel_proto.domains()) {
    GURL url = GURL(domain);
    if (url.is_valid() && url.SchemeIsHTTPOrHTTPS()) {
      cache_.emplace(url::Origin::Create(url), funnel);
    }
  }
}

bool FastCheckoutCapabilitiesFetcherImpl::IsTriggerFormSupported(
    const url::Origin& origin,
    autofill::FormSignature form_signature) {
  if (!cache_.contains(origin)) {
    base::UmaHistogramEnumeration(
        kUmaKeyCacheStateIsTriggerFormSupported,
        url_loader_ ? CacheStateForIsTriggerFormSupported::kFetchOngoing
                    : CacheStateForIsTriggerFormSupported::kEntryNotAvailable);
    return false;
  }

  bool is_supported = cache_.at(origin).trigger.contains(form_signature);
  base::UmaHistogramEnumeration(
      kUmaKeyCacheStateIsTriggerFormSupported,
      is_supported
          ? CacheStateForIsTriggerFormSupported::kEntryAvailableAndFormSupported
          : CacheStateForIsTriggerFormSupported::
                kEntryAvailableAndFormNotSupported);
  return is_supported;
}

base::flat_set<autofill::FormSignature>
FastCheckoutCapabilitiesFetcherImpl::GetFormsToFill(const url::Origin& origin) {
  if (!cache_.contains(origin)) {
    return {};
  }
  const FastCheckoutFunnel& funnel = cache_.at(origin);
  // All `FastCheckoutFunnel::trigger` and `FastCheckoutFunnel::fill` forms
  // should be attempted to be filled, in any order. For that reason, merge the
  // two sets into one set (`forms_to_fill`) and return it.
  base::flat_set<autofill::FormSignature> forms_to_fill = funnel.trigger;
  forms_to_fill.insert(funnel.fill.begin(), funnel.fill.end());
  return forms_to_fill;
}
