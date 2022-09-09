// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_H_

#include "base/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {
class FormSignature;
}  // namespace autofill

namespace url {
class Origin;
}  // namespace url

// A service that provides information about whether a form on a given origin
// is supported for FastCheckout flows. The service is used as one of several
// inputs that determine whether to offer a FastCheckout flow to a user
// interacting with an input form field.
// Availability is queried in a privacy-preserving manner by utilizing
// `AutofillAssistant::GetCapabilitiesByHashPrefix()`.
class FastCheckoutCapabilitiesFetcher : public KeyedService {
 public:
  // Callback with a `bool` parameter that indicates whether the availability
  // request was successful. `false` indicates an RPC error.
  using Callback = base::OnceCallback<void(bool)>;

  // Sends a request to determine which (if any) forms are supported for
  // FastCheckout on `origin`. Calls `callback` to indicate the success of
  // the request (and not whether origin is supported).
  virtual void FetchAvailability(const url::Origin& origin,
                                 Callback callback) = 0;
  // Checks whether a form with `form_signature` on `origin` is supported
  // for FastCheckout. Requires `FetchAvailability` to have been completed
  // for this origin or will return `false` otherwise.
  virtual bool IsTriggerFormSupported(
      const url::Origin& origin,
      autofill::FormSignature form_signature) = 0;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_CAPABILITIES_FETCHER_H_
