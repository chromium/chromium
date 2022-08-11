// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_IMPL_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_IMPL_H_

#include <memory>

#include "base/callback_forward.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {
class FormSignature;
}  // namespace autofill

namespace autofill_assistant {
class AutofillAssistant;
}  // namespace autofill_assistant

namespace url {
class Origin;
}  // namespace url

class FastCheckoutCapabilitiesFetcherImpl
    : public FastCheckoutCapabilitiesFetcher {
 public:
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
  // An `AutofillAssistant` instance to gain access to
  // `GetCapabilitiesByHashPrefix` RPC calls.
  const std::unique_ptr<autofill_assistant::AutofillAssistant>
      autofill_assistant_;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_IMPL_H_
