// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_VERIFY_TRUST_API_V2_H_
#define CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_VERIFY_TRUST_API_V2_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "chrome/browser/extensions/api/platform_keys/verify_trust_api_base.h"
#include "content/public/browser/browser_thread.h"
#include "net/cert/cert_verify_result.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// An implementation of VerifyTrustApiBase which has option
// of fetching parent certificate using NetworkContext
class VerifyTrustApiV2 : public VerifyTrustApiBase {
 public:
  explicit VerifyTrustApiV2(content::BrowserContext* context);

  VerifyTrustApiV2(const VerifyTrustApiV2&) = delete;
  VerifyTrustApiV2& operator=(const VerifyTrustApiV2&) = delete;

  ~VerifyTrustApiV2() override;

  // VerifyTrustApiBase:
  void Verify(Params params,
              const std::string& extension_id,
              VerifyCallback callback) override;

 private:
  void OnCertChainCreated(std::string hostname,
                          const std::string& extension_id,
                          VerifyCallback callback,
                          base::expected<scoped_refptr<net::X509Certificate>,
                                         std::string> cert_chain);

  // Calls `ui_callback` with the given parameters.
  void OnVerifyCert(VerifyCallback callback,
                    int verify_result,
                    const net::CertVerifyResult& result,
                    bool pkp_bypassed);

  raw_ptr<content::BrowserContext> browser_context_;
  base::WeakPtrFactory<VerifyTrustApiV2> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_VERIFY_TRUST_API_V2_H_
