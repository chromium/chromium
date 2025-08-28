// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_VERIFY_TRUST_API_BASE_H_
#define CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_VERIFY_TRUST_API_BASE_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "chrome/common/extensions/api/platform_keys.h"

namespace extensions {
class VerifyTrustApiBase {
 public:
  // Will be called with `return_value` set to the verification result (net::OK
  // if the certificate is trusted, otherwise a net error code) and
  // `cert_status` to the bitwise-OR of CertStatus flags. If an error occurred
  // during processing the parameters, `error` is set to an english error
  // message and `return_value` and `cert_status` must be ignored.
  using VerifyCallback = base::OnceCallback<
      void(const std::string& error, int return_value, int cert_status)>;
  using Params = api::platform_keys::VerifyTLSServerCertificate::Params;

  // Verifies the server certificate as described by `params` for the
  // extension with id `extension_id`. When verification is complete
  // (successful or not), the result will be passed to `callback`.
  //
  // Note: It is safe to delete this object while there are still
  // outstanding operations. However, if this happens, `callback`
  // will NOT be called.
  virtual void Verify(Params params,
                      const std::string& extension_id,
                      VerifyCallback callback) = 0;

  virtual ~VerifyTrustApiBase() = default;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PLATFORM_KEYS_VERIFY_TRUST_API_BASE_H_
