// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_impl.h"

#include <memory>

#include "base/callback.h"
#include "base/notreached.h"

#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "components/autofill/core/common/signatures.h"

#include "url/origin.h"

FastCheckoutCapabilitiesFetcherImpl::FastCheckoutCapabilitiesFetcherImpl() =
    default;

FastCheckoutCapabilitiesFetcherImpl::~FastCheckoutCapabilitiesFetcherImpl() =
    default;

void FastCheckoutCapabilitiesFetcherImpl::FetchAvailability(
    const url::Origin& origin,
    Callback callback) {
  NOTIMPLEMENTED();
  std::move(callback).Run(false);
}

bool FastCheckoutCapabilitiesFetcherImpl::IsTriggerFormSupported(
    const url::Origin& origin,
    autofill::FormSignature form_signature) {
  if (base::FeatureList::IsEnabled(
          features::kForceEnableFastCheckoutCapabilities)) {
    return true;
  }

  NOTIMPLEMENTED();
  return false;
}
