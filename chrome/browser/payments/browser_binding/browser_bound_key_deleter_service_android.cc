// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/browser_binding/browser_bound_key_deleter_service_android.h"

#include <memory>
#include <utility>

#include "base/barrier_callback.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#include "components/payments/content/browser_binding/passkey_browser_binder.h"
#include "components/payments/content/web_payments_web_data_service.h"
#include "components/webauthn/android/internal_authenticator_android.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#include "third_party/blink/public/common/features.h"

namespace {

using webauthn::InternalAuthenticator;
using webauthn::InternalAuthenticatorAndroid;

}  // namespace

namespace payments {

namespace {
// Type for a map from string (relying party) to a vector of BBK metadata.
using RelyingPartyToBrowserBoundKeyMetadata =
    base::flat_map<std::string, std::vector<BrowserBoundKeyMetadata>>;

RelyingPartyToBrowserBoundKeyMetadata GroupByRelyingPartyId(
    std::vector<BrowserBoundKeyMetadata> bbk_metas) {
  RelyingPartyToBrowserBoundKeyMetadata grouped;
  for (auto& bbk_meta : bbk_metas) {
    grouped[bbk_meta.passkey.relying_party_id].push_back(std::move(bbk_meta));
  }
  return grouped;
}

// Flattens a vector of vector of metadata to a vector of metadata.
std::vector<BrowserBoundKeyMetadata> FlattenBrowserBoundKeyMetadata(
    std::vector<std::vector<BrowserBoundKeyMetadata>> nested) {
  std::vector<BrowserBoundKeyMetadata> flattened;
  for (auto& inner : nested) {
    std::ranges::move(inner, std::back_inserter(flattened));
  }
  return flattened;
}

std::vector<BrowserBoundKeyMetadata> RemoveMatchingCredentialIds(
    std::vector<BrowserBoundKeyMetadata> bbk_metas,
    std::vector<std::vector<uint8_t>> matching_credential_ids) {
  std::erase_if(bbk_metas, [&matching_credential_ids](auto& bbk_meta) {
    return base::Contains(matching_credential_ids,
                          bbk_meta.passkey.credential_id);
  });
  return bbk_metas;
}

}  // namespace

BrowserBoundKeyDeleterServiceAndroid::BrowserBoundKeyDeleterServiceAndroid(
    scoped_refptr<WebPaymentsWebDataService> web_data_service,
    scoped_refptr<BrowserBoundKeyStore> browser_bound_key_store)
    : web_data_service_(web_data_service),
      browser_bound_key_store_(browser_bound_key_store) {}

BrowserBoundKeyDeleterServiceAndroid::~BrowserBoundKeyDeleterServiceAndroid() =
    default;

void BrowserBoundKeyDeleterServiceAndroid::RemoveInvalidBBKs() {
  // The WebPaymentsWebDataService, authenticator and BrowserBoundKeyStore are
  // required to remove browser bound keys.
  if (!web_data_service_ || !browser_bound_key_store_) {
    return;
  }

  auto authenticator = authenticator_for_testing_
                           ? std::move(authenticator_for_testing_)
                           : std::make_unique<InternalAuthenticatorAndroid>(
                                 /*render_frame_host=*/nullptr);

  if (!authenticator->IsGetMatchingCredentialIdsSupported()) {
    // SPC (on Android) requires GetMatchingCredentialIds, so BBKs are not
    // relevant when this API is not supported.
    return;
  }

  if (!base::FeatureList::IsEnabled(
          blink::features::kSecurePaymentConfirmationBrowserBoundKeys)) {
    return;
  }

  auto passkey_browser_binder =
      passkey_browser_binder_for_testing_
          ? std::move(passkey_browser_binder_for_testing_)
          : std::make_unique<PasskeyBrowserBinder>(browser_bound_key_store_,
                                                   web_data_service_);

  passkey_browser_binder->GetAllBrowserBoundKeys(base::BindOnce(
      &BrowserBoundKeyDeleterServiceAndroid::FilterAndDeleteInvalidBBKs,
      weak_ptr_factory_.GetWeakPtr(), std::move(authenticator),
      std::move(passkey_browser_binder)));
}

void BrowserBoundKeyDeleterServiceAndroid::SetInternalAuthenticatorForTesting(
    std::unique_ptr<webauthn::InternalAuthenticator> authenticator) {
  authenticator_for_testing_ = std::move(authenticator);
}

void BrowserBoundKeyDeleterServiceAndroid::SetPasskeyBrowserBinderForTesting(
    std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder) {
  passkey_browser_binder_for_testing_ = std::move(passkey_browser_binder);
}

void BrowserBoundKeyDeleterServiceAndroid::FilterAndDeleteInvalidBBKs(
    std::unique_ptr<InternalAuthenticator> authenticator,
    std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder,
    std::vector<BrowserBoundKeyMetadata> browser_bound_keys) {
  if (browser_bound_keys.empty()) {
    // No BBKs to be deleted. Destroy the `InternalAuthenticator` and
    // `PasskeyBrowserBinder` instances.
    authenticator.reset();
    passkey_browser_binder.reset();
    return;
  }

  RelyingPartyToBrowserBoundKeyMetadata relying_party_to_bbk_metas =
      GroupByRelyingPartyId(std::move(browser_bound_keys));
  auto barrier_callback =
      base::BarrierCallback<std::vector<BrowserBoundKeyMetadata>>(
          relying_party_to_bbk_metas.size(),
          base::BindOnce(&FlattenBrowserBoundKeyMetadata)
              .Then(
                  base::BindOnce(&PasskeyBrowserBinder::DeleteBrowserBoundKeys,
                                 base::Unretained(passkey_browser_binder.get()),
                                 /*callback=*/
                                 base::BindOnce(
                                     [](std::unique_ptr<PasskeyBrowserBinder>
                                            passkey_browser_binder) {
                                       // This callback runs after BBK metadata
                                       // has been deleted.
                                       // Destroy the unique_ptr
                                       // passkey_browser_binder. Reset here
                                       // explicitly for emphasis. Note that the
                                       // passkey_browser_binder would be reset
                                       // regardless by going out of scope.
                                       passkey_browser_binder.reset();
                                     },
                                     std::move(passkey_browser_binder)))));

  for (std::pair<std::string, std::vector<BrowserBoundKeyMetadata>>& entry :
       relying_party_to_bbk_metas) {
    auto stored_credential_ids =
        base::ToVector(entry.second, [](const BrowserBoundKeyMetadata& bbk) {
          return bbk.passkey.credential_id;
        });
    authenticator->GetMatchingCredentialIds(
        entry.first, stored_credential_ids,
        /*require_third_party_payment_bit=*/false,
        base::BindOnce(&RemoveMatchingCredentialIds, std::move(entry.second))
            .Then(barrier_callback));
  }

  // Destroy the unique_ptr authenticator. Reset here explicitly for emphasis.
  // Note that the authenticator would be reset regardless by going out of
  // scope.
  authenticator.reset();
}

}  // namespace payments
