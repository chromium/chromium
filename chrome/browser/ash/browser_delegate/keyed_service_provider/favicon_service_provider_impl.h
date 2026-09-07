// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_FAVICON_SERVICE_PROVIDER_IMPL_H_
#define CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_FAVICON_SERVICE_PROVIDER_IMPL_H_

#include "chromeos/ash/components/favicon/favicon_service_provider.h"

class AccountId;

namespace favicon {
class FaviconService;
}  // namespace favicon

namespace ash {

// //chrome-side implementation of FaviconServiceProvider. Wraps
// //chrome/browser/favicon's Profile-keyed FaviconServiceFactory so ChromeOS
// callers can reach the service through the chromeos-side interface.
class FaviconServiceProviderImpl : public FaviconServiceProvider {
 public:
  FaviconServiceProviderImpl();
  FaviconServiceProviderImpl(const FaviconServiceProviderImpl&) = delete;
  FaviconServiceProviderImpl& operator=(const FaviconServiceProviderImpl&) =
      delete;
  ~FaviconServiceProviderImpl() override;

  // FaviconServiceProvider:
  favicon::FaviconService* Find(const AccountId& account_id) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_BROWSER_DELEGATE_KEYED_SERVICE_PROVIDER_FAVICON_SERVICE_PROVIDER_IMPL_H_
