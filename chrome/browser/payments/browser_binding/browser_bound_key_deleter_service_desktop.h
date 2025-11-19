// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAYMENTS_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_SERVICE_DESKTOP_H_
#define CHROME_BROWSER_PAYMENTS_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_SERVICE_DESKTOP_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/payments/browser_binding/browser_bound_key_deleter_service.h"

class LocalCredentialManagement;

namespace device {
class DiscoverableCredentialMetadata;
}

namespace payments {

struct BrowserBoundKeyMetadata;
class BrowserBoundKeyStore;
class PasskeyBrowserBinder;
class WebPaymentsWebDataService;

class BrowserBoundKeyDeleterServiceDesktop
    : public BrowserBoundKeyDeleterService {
 public:
  BrowserBoundKeyDeleterServiceDesktop(
      scoped_refptr<WebPaymentsWebDataService> web_data_service,
      scoped_refptr<BrowserBoundKeyStore> browser_bound_key_store,
      std::unique_ptr<LocalCredentialManagement> local_credential_management);

  ~BrowserBoundKeyDeleterServiceDesktop() override;

  // BrowserBoundKeyDeleterService:
  void RemoveInvalidBBKs() override;

  // Sets a PasskeyBrowserBinder to be used for testing. If this is not set, a
  // new PasskeyBrowserBinder will be created in `RemoveInvalidBBKs()`.
  void SetPasskeyBrowserBinderForTesting(
      std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder);

 private:
  void FilterAndDeleteInvalidBBKs(
      std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder,
      std::vector<BrowserBoundKeyMetadata> browser_bound_keys);

  void OnEnumerateComplete(
      std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder,
      std::vector<BrowserBoundKeyMetadata> browser_bound_keys,
      std::optional<std::vector<device::DiscoverableCredentialMetadata>>
          credentials);

  scoped_refptr<WebPaymentsWebDataService> web_data_service_;
  scoped_refptr<BrowserBoundKeyStore> browser_bound_key_store_;
  std::unique_ptr<LocalCredentialManagement> local_credential_management_;

  std::unique_ptr<PasskeyBrowserBinder> passkey_browser_binder_for_testing_;

  base::WeakPtrFactory<BrowserBoundKeyDeleterServiceDesktop> weak_ptr_factory_{
      this};
};

}  // namespace payments

#endif  // CHROME_BROWSER_PAYMENTS_BROWSER_BINDING_BROWSER_BOUND_KEY_DELETER_SERVICE_DESKTOP_H_
