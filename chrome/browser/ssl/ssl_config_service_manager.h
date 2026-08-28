// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SSL_CONFIG_SERVICE_MANAGER_H_
#define CHROME_BROWSER_SSL_SSL_CONFIG_SERVICE_MANAGER_H_

#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/ssl/ssl_config_service.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "services/network/public/mojom/ssl_config.mojom.h"

class PrefService;
class PrefRegistrySimple;

// This is defined as a standalone struct so that it can be forward-declared.
// Otherwise it would be cleaner to have it nested inside
// SSLConfigServiceManager.
struct SSLConfigServiceMtcLandmarkInfo {
  // The time after which these Trust Anchor IDs should no longer be used.
  base::Time max_usable_time;

  // A list of MTC Trust Anchor IDs which should contain at least one landmark
  // relative IDs, and also any standalone IDs for MTC CAs that did not have
  // landmark data.
  std::vector<std::vector<uint8_t>>
      mtc_landmark_and_standalone_trust_anchor_ids;
};

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
      base::span<const std::vector<uint8_t>> classic_trust_anchor_ids,
      base::span<const std::vector<uint8_t>>
          mtc_standalone_only_trust_anchor_ids,
      std::optional<SSLConfigServiceMtcLandmarkInfo> mtc_landmark_info);

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

  // Initializes the `trust_anchor_ids_` and `time_bound_trust_anchor_ids_`
  // from the provided inputs.
  void InitializeTrustAnchorIDs(
      base::span<const std::vector<uint8_t>> classic_trust_anchor_ids,
      base::span<const std::vector<uint8_t>>
          mtc_standalone_only_trust_anchor_ids,
      std::optional<SSLConfigServiceMtcLandmarkInfo> mtc_landmark_info);

  PrefChangeRegistrar local_state_change_registrar_;

  // The local_state prefs.
  BooleanPrefMember rev_checking_enabled_;
  BooleanPrefMember rev_checking_required_local_anchors_;
  StringPrefMember ssl_version_min_;
  StringPrefMember ssl_version_max_;
  StringListPrefMember h2_client_cert_coalescing_host_patterns_;
  BooleanPrefMember ech_enabled_;
  StringPrefMember key_exchange_compliance_;
  StringPrefMember tls13_cipher_compliance_;

  // The cached list of disabled SSL cipher suites.
  std::vector<uint16_t> disabled_cipher_suites_;

  mojo::RemoteSet<network::mojom::SSLConfigClient> ssl_config_client_set_;

  // The latest sets of Trust Anchor IDs either from the compiled-in root
  // store, or configured via UpdateTrustAnchorIDs(). These are used to set the
  // initial set of Trust Anchor IDs on newly created network contexts to the
  // latest ones.
  std::vector<uint8_t> trust_anchor_ids_;
  std::optional<net::TimeBoundTrustAnchorIDs> time_bound_trust_anchor_ids_;
};

#endif  // CHROME_BROWSER_SSL_SSL_CONFIG_SERVICE_MANAGER_H_
