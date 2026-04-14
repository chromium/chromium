// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_ENTRA_PROVIDER_ANDROID_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_ENTRA_PROVIDER_ANDROID_H_

#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider.h"

namespace enterprise_auth {

class EntraProviderAndroid : public enterprise_auth::PlatformAuthProvider {
 public:
  EntraProviderAndroid();

  ~EntraProviderAndroid() override;

  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: (
  // org.chromium.chrome.browser.enterprise.platform_auth.entra_provider_android)
  enum class TokenReadResult {
    kOk = 0,
    kUnexpectedError,
    kNoBrokerRegistered,
    kSignatureVerificationFailed,
    kInvalidBundleFormat,
    kNoBundleResult,
    kBundleResultContainsEntraError,
    kBundleResultContainsOsError,
    kUnexpectedPackageProvider,
    kMax = kUnexpectedPackageProvider
  };

  using OnJavaReadTokensCallback = base::OnceCallback<void(
      enterprise_auth::EntraProviderAndroid::TokenReadResult,
      std::string)>;

  inline void SetMockJavaReadTokensForTesting(
      base::RepeatingCallback<void(
          base::OnceCallback<void(TokenReadResult, std::string)>)> callback) {
    mock_java_read_tokens_ = std::move(callback);
  }

  // enterprise_auth::PlatformAuthProvider implementation.
  bool SupportsOriginFiltering() override;
  void FetchOrigins(FetchOriginsCallback on_fetch_complete) override;
  void GetData(const GURL& url,
               PlatformAuthProviderManager::GetDataCallback callback) override;

 private:
  // Handles the results read from the Android OS API.
  // On success `result` contains the JSON with the authentication headers.
  // On failure `result` contains a debug message about the error.
  void OnJavaHeadersRead(PlatformAuthProviderManager::GetDataCallback callback,
                         TokenReadResult result_code,
                         std::string result);

  void ParseJsonHeaders(PlatformAuthProviderManager::GetDataCallback callback,
                        std::string_view headers_raw_json);

  SEQUENCE_CHECKER(sequence_checker);
  bool sso_disabled_ = false;

  base::RepeatingCallback<void(OnJavaReadTokensCallback)>
      mock_java_read_tokens_;

  base::WeakPtrFactory<EntraProviderAndroid> weak_ptr_factory_{this};
};

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_ENTRA_PROVIDER_ANDROID_H_
