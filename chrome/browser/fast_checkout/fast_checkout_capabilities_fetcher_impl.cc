// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_impl.h"

#include <memory>

#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "chrome/browser/fast_checkout/fast_checkout_funnels.pb.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/origin.h"

namespace {
constexpr int kMaxDownloadSizeInBytes = 10 * 1024;
constexpr char kFastCheckoutFunnelsUrl[] =
    "https://www.gstatic.com/autofill/fast_checkout/funnels.binarypb";
constexpr base::TimeDelta kCacheTimeout(base::Minutes(10));
constexpr base::TimeDelta kFetchTimeout(base::Seconds(3));
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
                     base::Unretained(this)),
      kMaxDownloadSizeInBytes);
}

bool FastCheckoutCapabilitiesFetcherImpl::IsCacheStale() const {
  return last_fetch_timestamp_.is_null() ||
         base::TimeTicks::Now() - last_fetch_timestamp_ >= kCacheTimeout;
}

void FastCheckoutCapabilitiesFetcherImpl::OnFetchComplete(
    std::unique_ptr<std::string> response_body) {
  // TODO(crbug.com/1334642): Log duration.

  int net_error = url_loader_->NetError();

  // Reset `url_loader_` so that another request could be made.
  url_loader_.reset();
  last_fetch_timestamp_ = base::TimeTicks::Now();

  if (net_error != net::OK) {
    // TODO(crbug.com/1334642): Log `url_loader_->NetError()`.
    return;
  }

  if (!response_body) {
    // TODO(crbug.com/1334642): Log no response received.
    return;
  }

  ::fast_checkout::FastCheckoutFunnels funnels;
  if (!funnels.ParseFromString(*response_body)) {
    // TODO(crbug.com/1334642): Log parsing error.
    return;
  }

  for (const ::fast_checkout::FastCheckoutFunnels_FastCheckoutFunnel&
           funnel_proto : funnels.funnels()) {
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
}

bool FastCheckoutCapabilitiesFetcherImpl::IsTriggerFormSupported(
    const url::Origin& origin,
    autofill::FormSignature form_signature) {
  if (base::FeatureList::IsEnabled(
          features::kForceEnableFastCheckoutCapabilities)) {
    return true;
  }

  if (!cache_.contains(origin)) {
    // TODO(crbug.com/1334642): Log `origin` not in `cache_`.
    return false;
  }

  bool is_supported = cache_.at(origin).trigger.contains(form_signature);
  // TODO(crbug.com/1334642): Log `is_supported`.
  return is_supported;
}
