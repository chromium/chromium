// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_H_
#define CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_H_

#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/scoped_observation.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/net/proxy_config_monitor.h"
#include "chrome/common/buildflags.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/storage_partition.h"
#include "net/net_buildflags.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom-forward.h"
#include "services/network/public/mojom/cert_verifier_service_updater.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
#include "chrome/browser/net/server_certificate_database.h"  // nogncheck
#endif

class PrefRegistrySimple;
class Profile;

namespace net {
class ClientCertStore;

// Enum that specifies which profiles are allowed to do
// ambient authentication.
enum class AmbientAuthAllowedProfileTypes {
  kRegularOnly = 0,
  kIncognitoAndRegular = 1,
  kGuestAndRegular = 2,
  kAll = 3,
};

}  // namespace net

namespace user_prefs {
class PrefRegistrySyncable;
}

// KeyedService that initializes and provides access to the NetworkContexts for
// a Profile. This will eventually replace ProfileIOData.
class ProfileNetworkContextService
    : public KeyedService,
      public content_settings::Observer,
      public content_settings::CookieSettings::Observer {
 public:
  explicit ProfileNetworkContextService(Profile* profile);

  ProfileNetworkContextService(const ProfileNetworkContextService&) = delete;
  ProfileNetworkContextService& operator=(const ProfileNetworkContextService&) =
      delete;

  ~ProfileNetworkContextService() override;

  // Configures the NetworkContextParams and the CertVerifierCreationParams for
  // the BrowserContext, using the specified parameters. An empty
  // |relative_partition_path| corresponds to the main network context.
  void ConfigureNetworkContextParams(
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params);

  // Update all of the profile_'s CertVerifierServices with certificates from
  // enterprise policies, and any user-added certificates if present.
  void UpdateAdditionalCertificates();

  struct CertificatePoliciesForView {
    CertificatePoliciesForView();
    ~CertificatePoliciesForView();
    CertificatePoliciesForView(CertificatePoliciesForView&&);
    CertificatePoliciesForView& operator=(CertificatePoliciesForView&& other);

    cert_verifier::mojom::AdditionalCertificatesPtr certificate_policies;

    bool is_include_system_trust_store_managed;

    std::vector<std::vector<uint8_t>> full_distrusted_certs;
  };

  // Get enterprise certificate policies for viewing by end users.
  CertificatePoliciesForView GetCertificatePolicyForView();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Packages up configuration info in |profile| and |cookie_settings| into a
  // mojo-friendly form.
  static network::mojom::CookieManagerParamsPtr CreateCookieManagerParams(
      Profile* profile,
      const content_settings::CookieSettings& cookie_settings);

  // Flushes a cached client certificate preference for |host| if |certificate|
  // doesn't match the cached certificate.
  void FlushCachedClientCertIfNeeded(
      const net::HostPortPair& host,
      const scoped_refptr<net::X509Certificate>& certificate);

  // Flushes a cached client certificate preference if |certificate| matches
  // the cached certificate.
  void FlushMatchingCachedClientCert(
      const scoped_refptr<net::X509Certificate>& certificate);

  // Flushes all pending proxy configuration changes.
  void FlushProxyConfigMonitorForTesting();

  static void SetDiscardDomainReliabilityUploadsForTesting(bool value);

  void set_client_cert_store_factory_for_testing(
      base::RepeatingCallback<std::unique_ptr<net::ClientCertStore>()>
          factory) {
    client_cert_store_factory_ = std::move(factory);
  }

  // Get platform ClientCertStore. May return nullptr.
  std::unique_ptr<net::ClientCertStore> CreateClientCertStore();

 private:
  FRIEND_TEST_ALL_PREFIXES(ProfileNetworkContextServiceBrowsertest,
                           DefaultCacheSize);
  FRIEND_TEST_ALL_PREFIXES(ProfileNetworkContextServiceDiskCacheBrowsertest,
                           DiskCacheSize);
  FRIEND_TEST_ALL_PREFIXES(
      ProfileNetworkContextServiceCertVerifierBuiltinPermissionsPolicyTest,
      Test);

  friend class AmbientAuthenticationTestHelper;

  // Checks |quic_allowed_|, and disables QUIC if needed.
  void DisableQuicIfNotAllowed();

  // Forwards changes to |pref_accept_language_| to the NetworkContext, after
  // formatting them as appropriate.
  void UpdateAcceptLanguage();

  // Computes appropriate value of Accept-Language header based on
  // |pref_accept_language_|
  std::string ComputeAcceptLanguage() const;

  void UpdateReferrersEnabled();

  // Gets the current CTPolicy from preferences.
  network::mojom::CTPolicyPtr GetCTPolicy();

  // Update the CTPolicy for the given NetworkContexts.
  void UpdateCTPolicyForContexts(
      const std::vector<network::mojom::NetworkContext*>& contexts);

  // Update the CTPolicy for the all of profiles_'s NetworkContexts.
  void UpdateCTPolicy();

  void ScheduleUpdateCTPolicy();

  void ScheduleUpdateCertificatePolicy();

  // Get the current certificate policies from preferences.
  cert_verifier::mojom::AdditionalCertificatesPtr GetCertificatePolicy(
      const base::FilePath& storage_partition_path);

#if BUILDFLAG(CHROME_ROOT_STORE_CERT_MANAGEMENT_UI)
  // Like UpdateAdditionalCertificates, but also includes the passed in user
  // added certificates.
  void UpdateAdditionalCertificatesWithUserAddedCerts(
      std::vector<net::ServerCertificateDatabase::CertInformation> cert_infos);
#endif

  bool ShouldSplitAuthCacheByNetworkIsolationKey() const;
  void UpdateSplitAuthCacheByNetworkIsolationKey();

  void UpdateCorsNonWildcardRequestHeadersSupport();

#if BUILDFLAG(ENABLE_REPORTING)
  base::flat_map<std::string, GURL> GetEnterpriseReportingEndpoints() const;
  void UpdateEnterpriseReportingEndpoints();
#endif

  // Creates parameters for the NetworkContext. Use |in_memory| instead of
  // |profile_->IsOffTheRecord()| because sometimes normal profiles want off the
  // record partitions (e.g. for webview tag).
  void ConfigureNetworkContextParamsInternal(
      bool in_memory,
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params,
      cert_verifier::mojom::CertVerifierCreationParams*
          cert_verifier_creation_params);

  // Returns the path for a given storage partition.
  base::FilePath GetPartitionPath(
      const base::FilePath& relative_partition_path);

  // content_settings::Observer:
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type) override;

  // content_settings::CookieSettings::Observer:
  void OnThirdPartyCookieBlockingChanged(
      bool block_third_party_cookies) override;
  void OnMitigationsEnabledFor3pcdChanged(bool enable) override;
  void OnTrackingProtectionEnabledFor3pcdChanged(bool enable) override;

  const raw_ptr<Profile> profile_;

  ProxyConfigMonitor proxy_config_monitor_;

  BooleanPrefMember quic_allowed_;
  StringPrefMember pref_accept_language_;
  BooleanPrefMember enable_referrers_;
  PrefChangeRegistrar pref_change_registrar_;

  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  base::ScopedObservation<content_settings::CookieSettings,
                          content_settings::CookieSettings::Observer>
      cookie_settings_observation_{this};

  // Used to post schedule CT and Certificate policy updates
  base::OneShotTimer ct_policy_update_timer_;
  base::OneShotTimer cert_policy_update_timer_;

  // Used for testing.
  base::RepeatingCallback<std::unique_ptr<net::ClientCertStore>()>
      client_cert_store_factory_;
};

#endif  // CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_H_
