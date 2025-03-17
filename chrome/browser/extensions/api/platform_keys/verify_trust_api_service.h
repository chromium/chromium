// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_VERIFY_TRUST_API_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_VERIFY_TRUST_API_SERVICE_H_

#include <cstddef>
#include <optional>

#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/platform_keys/verify_trust_api_base.h"
#include "chrome/browser/extensions/api/platform_keys/verify_trust_api_v1.h"
#include "chrome/browser/extensions/api/platform_keys/verify_trust_api_v2.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// Service which decides which implementation of
// VerifyTrustApiBase to use based on `kVerifyTLSServerCertificateUseNetFetcher`
// from chrome/browser/extensions/api/platform_keys/verify_trust_api_base.h
class VerifyTrustApiService : public BrowserContextKeyedAPI,
                              public VerifyTrustApiBase {
 public:
  // Consumers should use the factory instead of this constructor.
  explicit VerifyTrustApiService(content::BrowserContext* context);

  VerifyTrustApiService(const VerifyTrustApiService&) = delete;
  VerifyTrustApiService& operator=(const VerifyTrustApiService&) = delete;
  ~VerifyTrustApiService() override;

  // VerifyTrustApiBase:
  void Verify(Params params,
              const std::string& extension_id,
              VerifyCallback callback) override;

  // BrowserContextKeyedAPI:
  static BrowserContextKeyedAPIFactory<VerifyTrustApiService>*
  GetFactoryInstance();

 protected:
  static const bool kServiceRedirectedInIncognito = true;
  static const bool kServiceIsCreatedWithBrowserContext = false;
  static const bool kServiceIsNULLWhileTesting = true;

 private:
  friend class BrowserContextKeyedAPIFactory<VerifyTrustApiService>;

  // BrowserContextKeyedAPI implementation.
  static const char* service_name() { return "VerifyTrustAPI"; }

  raw_ptr<content::BrowserContext> browser_context_;
  std::unique_ptr<VerifyTrustApiBase> verify_trust_api_inner_;
};

template <>
void BrowserContextKeyedAPIFactory<
    VerifyTrustApiService>::DeclareFactoryDependencies();

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_VERIFY_TRUST_API_SERVICE_H_
