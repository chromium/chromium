// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/notreached.h"
#include "components/autofill/core/common/signatures.h"
#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

FastCheckoutCapabilitiesFetcherImpl::FastCheckoutCapabilitiesFetcherImpl(
    std::unique_ptr<autofill_assistant::AutofillAssistant> autofill_assistant)
    : autofill_assistant_(std::move(autofill_assistant)) {}

FastCheckoutCapabilitiesFetcherImpl::~FastCheckoutCapabilitiesFetcherImpl() =
    default;

void FastCheckoutCapabilitiesFetcherImpl::FetchAvailability(
    const url::Origin& origin,
    Callback callback) {
  // TODO(crbug.com/1350456): Implement.
  NOTIMPLEMENTED();
}

bool FastCheckoutCapabilitiesFetcherImpl::IsTriggerFormSupported(
    const url::Origin& origin,
    autofill::FormSignature form_signature) {
  // TODO(crbug.com/1350456): Implement.
  NOTIMPLEMENTED();
  return false;
}
