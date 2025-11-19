// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/payments/browser_binding/browser_bound_key_deleter_service_desktop.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/time/time.h"
#include "chrome/browser/payments/browser_binding/browser_bound_key_deleter_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/webauthn/local_credential_management.h"
#include "components/payments/content/browser_binding/browser_bound_key_store.h"
#include "components/payments/content/browser_binding/passkey_browser_binder.h"
#include "components/payments/content/web_payments_web_data_service.h"
#include "content/public/browser/browser_context.h"
#include "third_party/blink/public/common/features.h"

namespace {
constexpr base::TimeDelta kBrowserBoundKeyExpirationDuration =
    9 * base::Days(30);
}

namespace payments {

BrowserBoundKeyDeleterServiceDesktop::BrowserBoundKeyDeleterServiceDesktop(
    scoped_refptr<WebPaymentsWebDataService> web_data_service,
    scoped_refptr<BrowserBoundKeyStore> browser_bound_key_store,
    std::unique_ptr<LocalCredentialManagement> local_credential_management)
    : web_data_service_(web_data_service),
      browser_bound_key_store_(browser_bound_key_store),
      local_credential_management_(std::move(local_credential_management)) {}

BrowserBoundKeyDeleterServiceDesktop::~BrowserBoundKeyDeleterServiceDesktop() =
    default;

void BrowserBoundKeyDeleterServiceDesktop::RemoveInvalidBBKs() {
  // The WebPaymentsWebDataService, BrowserBoundKeyStore, and
  // LocalCredentialManagement are required to remove browser bound keys.
  if (!web_data_service_ || !browser_bound_key_store_ ||
      !local_credential_management_) {
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
      &BrowserBoundKeyDeleterServiceDesktop::FilterAndDeleteInvalidBBKs,
      weak_ptr_factory_.GetWeakPtr(), std::move(passkey_browser_binder)));
}

void BrowserBoundKeyDeleterServiceDesktop::SetPasskeyBrowserBinderForTesting(
    std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder) {
  passkey_browser_binder_for_testing_ = std::move(passkey_browser_binder);
}

void BrowserBoundKeyDeleterServiceDesktop::FilterAndDeleteInvalidBBKs(
    std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder,
    std::vector<BrowserBoundKeyMetadata> browser_bound_keys) {
  if (browser_bound_keys.empty()) {
    // No BBKs to be deleted.
    return;
  }

  local_credential_management_->Enumerate(base::BindOnce(
      &BrowserBoundKeyDeleterServiceDesktop::OnEnumerateComplete,
      weak_ptr_factory_.GetWeakPtr(), std::move(passkey_browser_binder),
      std::move(browser_bound_keys)));
}

void BrowserBoundKeyDeleterServiceDesktop::OnEnumerateComplete(
    std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder,
    std::vector<BrowserBoundKeyMetadata> browser_bound_keys,
    std::optional<std::vector<device::DiscoverableCredentialMetadata>>
        credentials) {
  if (credentials.has_value()) {
    base::flat_set<BrowserBoundKeyMetadata::RelyingPartyAndCredentialId>
        valid_credentials;
    for (const auto& credential : credentials.value()) {
      valid_credentials.insert(
          BrowserBoundKeyMetadata::RelyingPartyAndCredentialId(
              credential.rp_id, credential.cred_id));
    }

    std::erase_if(browser_bound_keys, [&valid_credentials](auto& bbk_meta) {
      return base::Contains(valid_credentials, bbk_meta.passkey);
    });
  } else {
    // When finding local credentials is not supported on the platform, find
    // stale BBKs using the date-based method below.
    base::Time now_time = base::Time::NowFromSystemTime();
    std::erase_if(browser_bound_keys, [&now_time](auto& bbk_meta) {
      // The `last_used` field should never be null on desktop but keep the BBK
      // if null on the side of caution.
      return bbk_meta.last_used.is_null() ||
             (now_time - bbk_meta.last_used) <
                 kBrowserBoundKeyExpirationDuration;
    });
  }

  if (!browser_bound_keys.empty()) {
    passkey_browser_binder->DeleteBrowserBoundKeys(
        base::BindOnce(
            [](std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder) {
              // This callback runs after BBK metadata
              // has been deleted.
              // Destroy the unique_ptr
              // passkey_browser_binder. Reset here
              // explicitly for emphasis. Note that the
              // passkey_browser_binder would be reset
              // regardless by going out of scope.
              passkey_browser_binder.reset();
            },
            std::move(passkey_browser_binder)),
        std::move(browser_bound_keys));
  }
}

}  // namespace payments
