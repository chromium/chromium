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
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/net/proxy_config_monitor.h"
#include "components/content_settings/core/browser/content_settings_observer.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "net/net_buildflags.h"
#include "services/cert_verifier/public/mojom/cert_verifier_service_factory.mojom-forward.h"
#include "services/network/public/mojom/network_context.mojom.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"
#endif

class PrefRegistrySimple;
class Profile;
class TrialComparisonCertVerifierController;

namespace net {
class ClientCertStore;

// Enum that specifies which profiles are allowed to do
// ambient authentication.
enum class AmbientAuthAllowedProfileTypes {
  REGULAR_ONLY = 0,
  INCOGNITO_AND_REGULAR = 1,
  GUEST_AND_REGULAR = 2,
  ALL = 3,
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
      public content_settings::CookieSettings::Observer,
#if BUILDFLAG(ENABLE_EXTENSIONS)
      public extensions::ExtensionRegistryObserver,
#endif
      public privacy_sandbox::PrivacySandboxSettings::Observer {
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

#if BUILDFLAG(IS_CHROMEOS)
  void UpdateAdditionalCertificates();
#endif

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);
  static void RegisterLocalStatePrefs(PrefRegistrySimple* registry);

  // Packages up configuration info in |profile| and |cookie_settings| into a
  // mojo-friendly form.
  static network::mojom::CookieManagerParamsPtr CreateCookieManagerParams(
      Profile* profile,
      const content_settings::CookieSettings& cookie_settings);

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
  void UpdatePreconnect();

  // Gets the current CTPolicy from preferences.
  network::mojom::CTPolicyPtr GetCTPolicy();

  // Update the CTPolicy for the given NetworkContexts.
  void UpdateCTPolicyForContexts(
      const std::vector<network::mojom::NetworkContext*>& contexts);

  // Update the CTPolicy for the all of profiles_'s NetworkContexts.
  void UpdateCTPolicy();

  void ScheduleUpdateCTPolicy();

  bool ShouldSplitAuthCacheByNetworkIsolationKey() const;
  void UpdateSplitAuthCacheByNetworkIsolationKey();

  void UpdateCorsNonWildcardRequestHeadersSupport();

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

  // Populates |network_context_params| with initial additional server and
  // authority certificates for |relative_partition_path|.
  void PopulateInitialAdditionalCerts(
      const base::FilePath& relative_partition_path,
      network::mojom::NetworkContextParams* network_context_params);

  // content_settings::Observer:
  void OnContentSettingChanged(const ContentSettingsPattern& primary_pattern,
                               const ContentSettingsPattern& secondary_pattern,
                               ContentSettingsType content_type) override;

  // content_settings::CookieSettings::Observer:
  void OnThirdPartyCookieBlockingChanged(
      bool block_third_party_cookies) override;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // extensions::ExtensionRegistryObserver:
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;
#endif

  // PrivacySandboxSettings::Observer:
  void OnTrustTokenBlockingChanged(bool block_trust_tokens) override;
  void OnFirstPartySetsEnabledChanged(bool enabled) override;

  const raw_ptr<Profile> profile_;

  ProxyConfigMonitor proxy_config_monitor_;

  BooleanPrefMember quic_allowed_;
  StringPrefMember pref_accept_language_;
  BooleanPrefMember enable_referrers_;
  IntegerPrefMember preload_allowed_;
  PrefChangeRegistrar pref_change_registrar_;

  scoped_refptr<content_settings::CookieSettings> cookie_settings_;
  base::ScopedObservation<content_settings::CookieSettings,
                          content_settings::CookieSettings::Observer>
      cookie_settings_observation_{this};
  base::ScopedObservation<privacy_sandbox::PrivacySandboxSettings,
                          privacy_sandbox::PrivacySandboxSettings::Observer>
      privacy_sandbox_settings_observer_{this};

  // Used to post schedule CT policy updates
  base::OneShotTimer ct_policy_update_timer_;

#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)
  // Controls the cert verification trial. May be null if the trial is disabled
  // or not allowed for this profile.
  std::unique_ptr<TrialComparisonCertVerifierController>
      trial_comparison_cert_verifier_controller_;
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
  base::ScopedObservation<extensions::ExtensionRegistry,
                          ExtensionRegistryObserver>
      registry_observation_{this};
#endif

  // Used for testing.
  base::RepeatingCallback<std::unique_ptr<net::ClientCertStore>()>
      client_cert_store_factory_;
};

#endif  // CHROME_BROWSER_NET_PROFILE_NETWORK_CONTEXT_SERVICE_H_
