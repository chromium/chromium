// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/platform_keys/verify_trust_api_service.h"

#include "extensions/browser/extension_registry_factory.h"

namespace extensions {

VerifyTrustApiService::VerifyTrustApiService(content::BrowserContext* context)
    : browser_context_(context) {
  if (base::FeatureList::IsEnabled(kVerifyTLSServerCertificateUseNetFetcher)) {
    verify_trust_api_inner_ =
        std::make_unique<VerifyTrustApiV2>(browser_context_);
  } else {
    verify_trust_api_inner_ =
        std::make_unique<VerifyTrustApiV1>(browser_context_);
  }
}

VerifyTrustApiService::~VerifyTrustApiService() = default;

void VerifyTrustApiService::Verify(Params params,
                                   const std::string& extension_id,
                                   VerifyCallback callback) {
  verify_trust_api_inner_->Verify(std::move(params), extension_id,
                                  std::move(callback));
}

// static
BrowserContextKeyedAPIFactory<VerifyTrustApiService>*
VerifyTrustApiService::GetFactoryInstance() {
  static base::NoDestructor<
      BrowserContextKeyedAPIFactory<VerifyTrustApiService>>
      g_verify_trust_api_factory;

  return g_verify_trust_api_factory.get();
}

template <>
void BrowserContextKeyedAPIFactory<
    VerifyTrustApiService>::DeclareFactoryDependencies() {
  DependsOn(ExtensionsBrowserClient::Get()->GetExtensionSystemFactory());
  DependsOn(ExtensionRegistryFactory::GetInstance());
}

}  // namespace extensions
