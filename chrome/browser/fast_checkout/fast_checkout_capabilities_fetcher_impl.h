// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_IMPL_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_IMPL_H_

#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher.h"
#include "url/origin.h"

namespace autofill {
class FormSignature;
}  // namespace autofill

class FastCheckoutCapabilitiesFetcherImpl
    : public FastCheckoutCapabilitiesFetcher {
 public:
  FastCheckoutCapabilitiesFetcherImpl();
  ~FastCheckoutCapabilitiesFetcherImpl() override;

  FastCheckoutCapabilitiesFetcherImpl(
      const FastCheckoutCapabilitiesFetcherImpl&) = delete;
  FastCheckoutCapabilitiesFetcherImpl& operator=(
      const FastCheckoutCapabilitiesFetcherImpl&) = delete;

  // CapabilitiesFetcher:
  void FetchAvailability(const url::Origin& origin, Callback callback) override;
  bool IsTriggerFormSupported(const url::Origin& origin,
                              autofill::FormSignature form_signature) override;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_IMPL_H_
