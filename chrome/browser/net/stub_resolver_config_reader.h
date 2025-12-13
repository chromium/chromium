// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_STUB_RESOLVER_CONFIG_READER_H_
#define CHROME_BROWSER_NET_STUB_RESOLVER_CONFIG_READER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "chrome/browser/net/dns_over_https_config_source.h"
#include "components/prefs/pref_change_registrar.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/dns/public/dns_over_https_config.h"
#include "net/dns/public/dns_protocol.h"

class PrefRegistrySimple;
class PrefService;
class SecureDnsConfig;

// Retriever for Chrome configuration for the built-in DNS stub resolver.
class StubResolverConfigReader {
 public:
  // Detailed descriptions of the secure DNS mode. These values are logged to
  // UMA. Entries should not be renumbered and numeric values should never be
  // reused.
  //
  // LINT.IfChange(SecureDnsModeDetailsForHistogram)
  enum class SecureDnsModeDetailsForHistogram {
    // The mode is controlled by the user and is set to 'off'.
    kOffByUser = 0,
    // The mode is controlled via enterprise policy and is set to 'off'.
    kOffByEnterprisePolicy = 1,
    // Chrome detected a managed environment and forced the mode to 'off'.
    kOffByDetectedManagedEnvironment = 2,
    // Chrome detected parental controls and forced the mode to 'off'.
    kOffByDetectedParentalControls = 3,
    // The mode is controlled by the user and is set to 'automatic' (the
    // default mode).
    kAutomaticByUser = 4,
    // The mode is controlled via enterprise policy and is set to 'automatic'.
    kAutomaticByEnterprisePolicy = 5,
    // The mode is controlled by the user and is set to 'secure'.
    kSecureByUser = 6,
    // The mode is controlled via enterprise policy and is set to 'secure'.
    kSecureByEnterprisePolicy = 7,
    // The mode is controlled by the user and is set to 'automatic with
    // fallback to well-known DoH provider'.
    kAutomaticWithDohFallbackByUser = 8,
    // The mode is controlled via enterprise policy and is set to 'automatic
    // with fallback to well-known DoH provider'.
    kAutomaticWithDohFallbackByEnterprisePolicy = 9,
    kMaxValue = kAutomaticWithDohFallbackByEnterprisePolicy,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/net/histograms.xml:SecureDnsModeDetails)

  static constexpr base::TimeDelta kParentalControlsCheckDelay =
      base::Seconds(2);

  // |local_state| must outlive the created reader.
  explicit StubResolverConfigReader(PrefService* local_state,
                                    bool set_up_pref_defaults = true);

  StubResolverConfigReader(const StubResolverConfigReader&) = delete;
  StubResolverConfigReader& operator=(const StubResolverConfigReader&) = delete;

  virtual ~StubResolverConfigReader();

  static void RegisterPrefs(PrefRegistrySimple* registry);

  // Returns the current secure DNS resolver configuration.
  //
  // Initial checks for parental controls (which cause DoH to be disabled) may
  // be deferred for performance if called early during startup, if the
  // configuration is otherwise in AUTOMATIC mode. If this is undesirable, e.g.
  // because this is being called to populate the config UI, set
  // |force_check_parental_controls_for_automatic_mode| to force always waiting
  // for the parental controls check. If forcing the check when it had
  // previously been deferred, and the check discovers that DoH should be
  // disabled, the network service will be updated to disable DoH and ensure the
  // service behavior matches the config returned by this method.
  SecureDnsConfig GetSecureDnsConfiguration(
      bool force_check_parental_controls_for_automatic_mode);

  bool GetInsecureStubResolverEnabled();

  // Updates the network service with the current configuration.
  void UpdateNetworkService(bool record_metrics);

  // Returns true if there are any active machine level policies, if the
  // machine is domain joined (Windows), or any device or profile owner apps are
  // installed (Android). This special logic is used to disable DoH by default
  // for Desktop platforms and Android. ChromeOS is handled by the enterprise
  // policy field "default_for_enterprise_users".
  virtual bool ShouldDisableDohForManaged();

  // Returns true if there are parental controls detected on the device.
  virtual bool ShouldDisableDohForParentalControls();

#if BUILDFLAG(IS_ANDROID)
  // Updates the android owned state and network service if the device/prfile is
  // owned.
  void OnAndroidOwnedStateCheckComplete(bool has_profile_owner,
                                        bool has_device_owner);
#endif

  void OverrideParentalControlsForTesting(bool parental_controls_override) {
    parental_controls_testing_override_ = parental_controls_override;
  }

  // Overrides the default implementation for the class which monitors
  // DNS-over-HTTPS config changes. If `doh_source` is a null pointer, it clears
  // the override and resets to the default behaviour.
  void SetOverrideDnsOverHttpsConfigSource(
      std::unique_ptr<DnsOverHttpsConfigSource> doh_source);

#if BUILDFLAG(IS_WIN)
  // Set flag for testing Zero Trust DNS scenario
  static void SetZTDNSEnabledForTesting(bool is_ztdns_enabled_for_testing) {
    is_ztdns_enabled_for_testing_ = is_ztdns_enabled_for_testing;
  }

  static bool IsZTDNSEnabledForTesting() {
    return is_ztdns_enabled_for_testing_;
  }
#endif

  static std::vector<net::IPEndPoint> GetFallbackDohNameservers();

 private:
  void OnParentalControlsDelayTimer();

  // Updates network service if |update_network_service| or if necessary due to
  // first read of parental controls.
  SecureDnsConfig GetAndUpdateConfiguration(
      bool force_check_parental_controls_for_automatic_mode,
      bool record_metrics,
      bool update_network_service);

  // Returns the current config source for DNS-over-HTTPS settings. If
  // `SetOverrideDnsOverHttpsConfigSource` was called with a non-null value, it
  // returns the override config source; otherwise it returns the default
  // implementation.
  const DnsOverHttpsConfigSource* GetDnsOverHttpsConfigSource() const;

  bool GetHappyEyeballsV3Enabled() const;

  const raw_ptr<PrefService> local_state_;

  // Timer for deferred running of parental controls checks. Underling API calls
  // may be slow and run off-thread. Calling for the result is delayed to avoid
  // blocking during startup.
  base::OneShotTimer parental_controls_delay_timer_;
  // Whether or not parental controls have already been checked, either due to
  // expiration of the delay timer or because of a forced check.
  bool parental_controls_checked_ = false;

  std::optional<bool> parental_controls_testing_override_;

  std::unique_ptr<DnsOverHttpsConfigSource> default_doh_source_;
  std::unique_ptr<DnsOverHttpsConfigSource> override_doh_source_;

  // This object lives on the UI thread, but it's possible for it to be created
  // before BrowserMainLoop::CreateThreads() is called which would cause a
  // DCHECK for the UI thread to fail (as the UI thread hasn't been
  // named yet). Using a sequence checker works around this.
  SEQUENCE_CHECKER(sequence_checker_);

  PrefChangeRegistrar pref_change_registrar_;

#if BUILDFLAG(IS_ANDROID)
  // Whether or not an Android device or profile is owned.
  // A nullopt indicates this value has not been determined yet.
  std::optional<bool> android_has_owner_ = std::nullopt;
#endif

#if BUILDFLAG(IS_WIN)
  // Flag used for testing Zero Trust DNS scenario.
  static bool is_ztdns_enabled_for_testing_;
#endif

  base::WeakPtrFactory<StubResolverConfigReader> weak_factory_{this};
};

#endif  // CHROME_BROWSER_NET_STUB_RESOLVER_CONFIG_READER_H_
