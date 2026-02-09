// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/wallet_pass_access_manager_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager.h"
#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager_impl.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/keyed_service/core/keyed_service.h"

namespace autofill {

// static
WalletPassAccessManager* WalletPassAccessManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<WalletPassAccessManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
WalletPassAccessManagerFactory* WalletPassAccessManagerFactory::GetInstance() {
  static base::NoDestructor<WalletPassAccessManagerFactory> instance;
  return instance.get();
}

WalletPassAccessManagerFactory::WalletPassAccessManagerFactory()
    : ProfileKeyedServiceFactory(
          "WalletPassAccessManager",
          ProfileSelections::BuildRedirectedInIncognito()) {}

WalletPassAccessManagerFactory::~WalletPassAccessManagerFactory() = default;

std::unique_ptr<KeyedService>
WalletPassAccessManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kAutofillAiWalletPrivatePasses)) {
    return nullptr;
  }
  return std::make_unique<WalletPassAccessManagerImpl>();
}

}  // namespace autofill
