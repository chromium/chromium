// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CONFIG_SERVICE_MANAGER_H_
#define CHROME_BROWSER_SSL_SSL_CONFIG_SERVICE_MANAGER_H_

#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/ssl_config.mojom.h"

class PrefService;
class PrefRegistrySimple;

// Sends updated `network::mojom::SSLConfig`s to one or more
// `network::Mojom::SSLConfigClient`s. Not threadsafe.
class SSLConfigServiceManager {
 public:
  // Creates a new `SSLConfigServiceManager`. The lifetime of the `PrefService`
  // objects must be longer than that of the manager. Get SSL preferences from
  // `local_state`.
  explicit SSLConfigServiceManager(PrefService* local_state);

  SSLConfigServiceManager(const SSLConfigServiceManager&) = delete;
  SSLConfigServiceManager& operator=(const SSLConfigServiceManager&) = delete;

  static void RegisterPrefs(PrefRegistrySimple* registry);

  ~SSLConfigServiceManager();

  // The main entry point for obtaining an SSLConfig from this class:
  // Populates the `SSLConfig`-related members of `network_context_params`
  // (`initial_ssl_config` and `ssl_config_client_receiver`). Updated
  // `SSLConfig`s will be sent to the `NetworkContext` created with those params
  // whenever the configuration changes. Can be called more than once to inform
  // multiple `NetworkContext`s of changes.
  void AddToNetworkContextParams(
      network::mojom::NetworkContextParams* network_context_params);

  // Notifies SSLConfigClients that the given list of |trust_anchor_ids| and
  // |mtc_trust_anchor_ids| (lists of TLS Trust Anchor IDs in binary
  // representation) should now be trusted.  These would typically be provided
  // by component updater, to update/override a set of compiled-in trust anchor
  // IDs.
  void UpdateTrustAnchorIDs(
      std::vector<std::vector<uint8_t>> trust_anchor_ids,
      std::vector<std::vector<uint8_t>> mtc_trust_anchor_ids);

  // Computes the SSL compliance policy settings based on the given prefs and
  // feature state, and writes those settings into the appropriate fields in
  // `config`.
  static void ConfigureSSLComplianceSettings(
      const StringPrefMember& key_exchange_compliance_pref,
      const StringPrefMember& tls13_cipher_compliance_pref,
      network::mojom::SSLConfig* config);

  // Flushes all `SSLConfigClient` mojo pipes, to avoid races in tests.
  void FlushForTesting();

 private:
  // Callback for preference changes.  This will post the changes to the IO
  // thread with `SetNewSSLConfig`.
  void OnPreferenceChanged(PrefService* prefs, const std::string& pref_name);

  // Returns the current `SSLConfig` settings from preferences and other
  // applicable data sources. Assumes `disabled_cipher_suites_` is up-to-date,
  // but reads all other settings from live prefs.
  network::mojom::SSLConfigPtr GetNewSSLConfig() const;

  // Processes changes to the disabled cipher suites preference, updating the
  // cached list of parsed SSL/TLS cipher suites that are disabled.
  void OnDisabledCipherSuitesChange(PrefService* local_state);

  PrefChangeRegistrar local_state_change_registrar_;

  // The local_state prefs.
  BooleanPrefMember rev_checking_enabled_;
  BooleanPrefMember rev_checking_required_local_anchors_;
  StringPrefMember ssl_version_min_;
  StringPrefMember ssl_version_max_;
  StringListPrefMember h2_client_cert_coalescing_host_patterns_;
  BooleanPrefMember post_quantum_enabled_;
#if BUILDFLAG(IS_CHROMEOS)
  BooleanPrefMember device_post_quantum_enabled_;
#endif
  BooleanPrefMember ech_enabled_;
  StringPrefMember key_exchange_compliance_;
  StringPrefMember tls13_cipher_compliance_;

  // The cached list of disabled SSL cipher suites.
  std::vector<uint16_t> disabled_cipher_suites_;

  mojo::RemoteSet<network::mojom::SSLConfigClient> ssl_config_client_set_;
  // The latest set of Trust Anchor IDs configured via UpdateTrustAnchorIDs().
  // This is used to set the initial set of Trust Anchor IDs on newly created
  // network contexts to the latest ones. Note that this field can be set to a
  // non-null but empty value to override a non-empty compiled-in list of Trust
  // Anchor IDs with an empty list from the component updater.
  std::optional<std::vector<std::vector<uint8_t>>> trust_anchor_ids_;

  // Like `trust_anchor_ids_` but for MTC signatureless certs. (This is not
  // a std::optional since there is no built-in list of MTC ids to override.)
  std::vector<std::vector<uint8_t>> mtc_trust_anchor_ids_;
};

#endif  // CHROME_BROWSER_SSL_SSL_CONFIG_SERVICE_MANAGER_H_
