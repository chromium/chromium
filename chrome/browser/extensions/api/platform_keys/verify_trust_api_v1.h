// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_VERIFY_TRUST_API_V1_H_
#define CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_VERIFY_TRUST_API_V1_H_

#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/platform_keys/verify_trust_api_base.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace extensions {

// Original version of VerifyTrustApi which doesn't
// support net fetches of parent certificates.
class VerifyTrustApiV1 : public VerifyTrustApiBase,
                         public ExtensionRegistryObserver {
 public:
  explicit VerifyTrustApiV1(content::BrowserContext* context);
  ~VerifyTrustApiV1() override;

  // VerifyTrustApiBase:
  void Verify(Params params,
              const std::string& extension_id,
              VerifyCallback callback) override;

  // ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

 private:
  class IOPart;

  // Calls `ui_callback` with the given parameters.
  void FinishedVerificationOnUI(VerifyCallback ui_callback,
                                const std::string& error,
                                int return_value,
                                int cert_status);

  // Calls `ui_callback` on the UIThread with the given arguments.
  static void CallBackOnUI(VerifyCallback ui_callback,
                           const std::string& error,
                           int return_value,
                           int cert_status);

  // Created on the UIThread but must be used and destroyed only on the
  // IOThread.
  std::unique_ptr<IOPart, content::BrowserThread::DeleteOnIOThread> io_part_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      registry_observation_{this};
  base::WeakPtrFactory<VerifyTrustApiV1> weak_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_VERIFY_TRUST_API_V1_H_
