// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/wallet_pass_access_manager_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/network/autofill_ai/fake_wallet_pass_access_manager.h"
#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager.h"
#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager_impl.h"
#include "components/autofill/core/common/autofill_debug_features.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/wallet/core/browser/network/wallet_http_client_impl.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

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
          ProfileSelections::BuildRedirectedInIncognito()) {
  DependsOn(AutofillEntityDataManagerFactory::GetInstance());
  DependsOn(IdentityManagerFactory::GetInstance());
}

WalletPassAccessManagerFactory::~WalletPassAccessManagerFactory() = default;

std::unique_ptr<KeyedService>
WalletPassAccessManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kAutofillAiWalletPrivatePasses)) {
    return nullptr;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  EntityDataManager* data_manager =
      AutofillEntityDataManagerFactory::GetForProfile(profile);

  if (base::FeatureList::IsEnabled(
          autofill::features::debug::kFakeWalletApiResponses)) {
    return std::make_unique<FakeWalletPassAccessManager>(data_manager);
  }

  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);
  return std::make_unique<WalletPassAccessManagerImpl>(
      std::make_unique<wallet::WalletHttpClientImpl>(
          identity_manager, profile->GetURLLoaderFactory()),
      data_manager);
}

}  // namespace autofill
