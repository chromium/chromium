// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_ENTRA_PROVIDER_ANDROID_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_ENTRA_PROVIDER_ANDROID_H_

#include "base/functional/callback_forward.h"
#include "base/sequence_checker.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider.h"

namespace base {
class TimeTicks;
}

namespace enterprise_auth {

class EntraProviderAndroid : public enterprise_auth::PlatformAuthProvider {
 public:
  EntraProviderAndroid();

  ~EntraProviderAndroid() override;

  static constexpr char kAuthenticationResultHistogram[] =
      "Enterprise.AndroidEntraSso.Result";
  static constexpr char kDurationSuccessHistogram[] =
      "Enterprise.AndroidEntraSso.Duration.Success";
  static constexpr char kDurationFailureHistogram[] =
      "Enterprise.AndroidEntraSso.Duration.Failure";
  static constexpr char kDurationNoBrokerHistogram[] =
      "Enterprise.AndroidEntraSso.Duration.NoBroker";
  static constexpr char kFailureReasonHistogram[] =
      "Enterprise.AndroidEntraSso.FailureReason";
  static constexpr char kHeaderSkipReasonHistogram[] =
      "Enterprise.AndroidEntraSso.HeaderSkipReason";

  // These values are persisted to logs. Entries should not be renumbered
  // and numeric values should never be reused.
  //
  // LINT.IfChange(AuthenticationResult)
  enum class AuthenticationResult {
    kSuccessWithHeaders = 0,
    kSuccessWithNoHeaders = 1,
    kNoBrokerRegistered = 2,
    kFailure = 3,
    kMaxValue = kFailure
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:AndroidEntraSsoResult)

  // These values are persisted to logs. Entries should not be renumbered
  // and numeric values should never be reused.
  //
  // LINT.IfChange(Status)
  //
  // A Java counterpart will be generated for this enum.
  // GENERATED_JAVA_ENUM_PACKAGE: (
  // org.chromium.chrome.browser.enterprise.platform_auth.entra_provider_android)
  enum class Status {
    kUnexpectedError = 0,
    kSignatureVerificationFailed = 1,
    kInvalidBundleFormat = 2,
    kNoBundleResult = 3,
    kBundleResultContainsEntraError = 4,
    kBundleResultContainsOsError = 5,
    kUnexpectedPackageProvider = 6,
    kDisallowedDebugPackageProvider = 7,
    kTimeout = 8,
    kJsonParsingFailed = 9,
    kAllHeadersSkipped = 10,
    kMaxFailureReason = kAllHeadersSkipped,

    // The values below this line are not used by the histogram.
    kOk = 11,
    kNoBrokerRegistered = 12,

    kMaxValue = kNoBrokerRegistered,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:AndroidEntraSsoFailureReason)

  // These values are persisted to logs. Entries should not be renumbered
  // and numeric values should never be reused.
  //
  // LINT.IfChange(HeaderSkipReason)
  enum class HeaderSkipReason {
    kNamePrefixMismatch = 0,
    kInvalidHeaderName = 1,
    kInvalidValueFormat = 2,
    kInvalidHeaderValue = 3,
    kMaxValue = kInvalidHeaderValue,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:AndroidEntraSsoHeaderSkipReason)

  using OnJavaReadTokensCallback =
      base::OnceCallback<void(Status, std::string)>;

  inline void SetMockJavaReadTokensForTesting(
      base::RepeatingCallback<void(OnJavaReadTokensCallback)> callback) {
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
                         base::TimeTicks start_time,
                         Status status,
                         std::string result);

  void ParseJsonHeaders(PlatformAuthProviderManager::GetDataCallback callback,
                        std::string_view headers_raw_json,
                        base::TimeTicks start_time);

  SEQUENCE_CHECKER(sequence_checker);
  bool sso_disabled_ = false;

  base::RepeatingCallback<void(OnJavaReadTokensCallback)>
      mock_java_read_tokens_;

  base::WeakPtrFactory<EntraProviderAndroid> weak_ptr_factory_{this};
};

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_ENTRA_PROVIDER_ANDROID_H_
