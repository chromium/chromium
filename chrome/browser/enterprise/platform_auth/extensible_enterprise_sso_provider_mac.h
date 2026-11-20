// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_PROVIDER_MAC_H_
#define CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_PROVIDER_MAC_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider.h"
#include "chrome/browser/enterprise/platform_auth/platform_auth_provider_manager.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace enterprise_auth {

// Class that provides authentication functionalities from Extensible Enterprise
// SSO. This class relies on the SSO extensions installed on the device. When
// both `kPingSso` and `kExtensibleEnterpriseSsoOriginsFiltering` are enabled,
// origin filtering is supported and the origins are derived from the `URLs`
// entry of the installed `com.apple.extensiblesso` MDM payload; otherwise a
// platform API call is required to know if a URL is supported, so every request
// is passed to `GetData()`.
class ExtensibleEnterpriseSSOProvider : public PlatformAuthProvider {
 public:
  // Failure reasons before IdP authentication begins for an HTTP(S) request.
  // Recorded to `Enterprise.ExtensibleEnterpriseSSO.Failure.Reason`.
  //
  // These values are persisted to logs. Entries should not be renumbered
  // and numeric values should never be reused.
  //
  // LINT.IfChange(FailureReason)
  enum class FailureReason {
    kOsCannotPerformAuthorization = 0,
    kCapabilityCheckTimedOut = 1,
    kMaxValue = kCapabilityCheckTimedOut,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/enterprise/enums.xml:ExtensibleEnterpriseSSOFailureReason)

  struct Metrics {
    explicit Metrics(const std::string& host);
    ~Metrics();
    Metrics(const Metrics&) = delete;
    Metrics& operator=(const Metrics&) = delete;

    std::string host;
    base::Time start_time;
    base::Time end_time;
    bool url_is_supported = false;
    bool success = false;
    std::optional<FailureReason> failure_reason;
  };

  struct DelegateResult {
    DelegateResult(const std::string& name,
                   bool success,
                   net::HttpRequestHeaders headers = net::HttpRequestHeaders());
    ~DelegateResult();
    DelegateResult(const DelegateResult&) = delete;
    DelegateResult& operator=(const DelegateResult&) = delete;

    std::string name;
    bool success;
    net::HttpRequestHeaders headers;
  };

  ExtensibleEnterpriseSSOProvider();
  ~ExtensibleEnterpriseSSOProvider() override;
  ExtensibleEnterpriseSSOProvider(const ExtensibleEnterpriseSSOProvider&) =
      delete;
  ExtensibleEnterpriseSSOProvider& operator=(
      const ExtensibleEnterpriseSSOProvider&) = delete;

  // enterprise_auth::PlatformAuthProvider:
  bool SupportsOriginFiltering() override;
  void FetchOrigins(FetchOriginsCallback on_fetch_complete) override;
  void GetData(const GURL& url,
               PlatformAuthProviderManager::GetDataCallback callback) override;

 private:
  // The OS objects for a URL that an SSO extension can authenticate. Defined
  // in the .mm file so that this header stays plain C++.
  struct AuthProvider;

  // Returns the OS objects for `url` if an SSO extension can authenticate it,
  // or null otherwise. Can block on a synchronous IPC to the AppSSO service.
  static std::unique_ptr<AuthProvider> GetAuthProviderSupportingUrl(
      const GURL& url);

  // Continues `GetData()` on the UI thread once the capability check is done.
  // If it `timed_out`, the request continues without SSO headers. Otherwise,
  // `auth_provider` is null if the OS can't authenticate the URL.
  void OnCanPerformAuthorization(
      PlatformAuthProviderManager::GetDataCallback callback,
      std::unique_ptr<Metrics> metrics,
      bool timed_out,
      std::unique_ptr<AuthProvider> auth_provider);

  // Runs the blocking capability checks one at a time, so an unresponsive
  // AppSSO service blocks at most one worker thread.
  const scoped_refptr<base::SequencedTaskRunner> capability_check_task_runner_;

  base::WeakPtrFactory<ExtensibleEnterpriseSSOProvider> weak_factory_{this};
};

}  // namespace enterprise_auth

#endif  // CHROME_BROWSER_ENTERPRISE_PLATFORM_AUTH_EXTENSIBLE_ENTERPRISE_SSO_PROVIDER_MAC_H_
