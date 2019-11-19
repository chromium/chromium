// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ssl/ssl_config_service_manager.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_member.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/cert/cert_verifier.h"
#include "net/ssl/ssl_cipher_suite_names.h"
#include "net/ssl/ssl_config_service.h"
#include "url/url_canon.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace {

// Converts a ListValue of StringValues into a vector of strings. Any Values
// which cannot be converted will be skipped.
std::vector<std::string> ListValueToStringVector(const base::ListValue* value) {
  std::vector<std::string> results;
  results.reserve(value->GetSize());
  std::string s;
  for (auto it = value->begin(); it != value->end(); ++it) {
    if (!it->GetAsString(&s))
      continue;
    results.push_back(s);
  }
  return results;
}

// Parses a vector of cipher suite strings, returning a sorted vector
// containing the underlying SSL/TLS cipher suites. Unrecognized/invalid
// cipher suites will be ignored.
std::vector<uint16_t> ParseCipherSuites(
    const std::vector<std::string>& cipher_strings) {
  std::vector<uint16_t> cipher_suites;
  cipher_suites.reserve(cipher_strings.size());

  for (auto it = cipher_strings.begin(); it != cipher_strings.end(); ++it) {
    uint16_t cipher_suite = 0;
    if (!net::ParseSSLCipherString(*it, &cipher_suite)) {
      LOG(ERROR) << "Ignoring unrecognized or unparsable cipher suite: " << *it;
      continue;
    }
    cipher_suites.push_back(cipher_suite);
  }
  std::sort(cipher_suites.begin(), cipher_suites.end());
  return cipher_suites;
}

// Writes the SSL protocol version represented by a string to |version|. Returns
// false if the string is not recognized.
bool SSLProtocolVersionFromString(const std::string& version_str,
                                  network::mojom::SSLVersion* version) {
  if (version_str == switches::kSSLVersionTLSv1) {
    *version = network::mojom::SSLVersion::kTLS1;
    return true;
  }
  if (version_str == switches::kSSLVersionTLSv11) {
    *version = network::mojom::SSLVersion::kTLS11;
    return true;
  }
  if (version_str == switches::kSSLVersionTLSv12) {
    *version = network::mojom::SSLVersion::kTLS12;
    return true;
  }
  if (version_str == switches::kSSLVersionTLSv13) {
    *version = network::mojom::SSLVersion::kTLS13;
    return true;
  }
  return false;
}

// Given a vector of hostname patterns |patterns|, returns a vector containing
// the canonical form. Any entries which cannot be parsed are skipped.
std::vector<std::string> CanonicalizeHostnamePatterns(
    const std::vector<std::string>& patterns) {
  std::vector<std::string> out;
  out.reserve(patterns.size());
  for (base::StringPiece pattern : patterns) {
    std::string canon_pattern;
    url::Component canon_component;
    url::StdStringCanonOutput canon_output(&canon_pattern);
    if (!url::CanonicalizeHost(pattern.data(),
                               url::Component(0, pattern.size()), &canon_output,
                               &canon_component)) {
      continue;
    }
    canon_output.Complete();
    out.push_back(canon_pattern);
  }
  return out;
}

////////////////////////////////////////////////////////////////////////////////
//  SSLConfigServiceManagerPref

// The manager for holding and updating one or more
// network::mojom::SSLConfigClients.
class SSLConfigServiceManagerPref : public SSLConfigServiceManager {
 public:
  explicit SSLConfigServiceManagerPref(PrefService* local_state);
  ~SSLConfigServiceManagerPref() override {}

  // Register local_state SSL preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);

  void AddToNetworkContextParams(
      network::mojom::NetworkContextParams* network_context_params) override;

  void FlushForTesting() override;

 private:
  // Callback for preference changes.  This will post the changes to the IO
  // thread with SetNewSSLConfig.
  void OnPreferenceChanged(PrefService* prefs, const std::string& pref_name);

  // Returns the current SSLConfig settings from preferences. Assumes
  // |disabled_cipher_suites_| is up-to-date, but reads all other settings from
  // live prefs.
  network::mojom::SSLConfigPtr GetSSLConfigFromPrefs() const;

  // Processes changes to the disabled cipher suites preference, updating the
  // cached list of parsed SSL/TLS cipher suites that are disabled.
  void OnDisabledCipherSuitesChange(PrefService* local_state);

  PrefChangeRegistrar local_state_change_registrar_;

  // The local_state prefs.
  BooleanPrefMember rev_checking_enabled_;
  BooleanPrefMember rev_checking_required_local_anchors_;
  BooleanPrefMember tls13_hardening_for_local_anchors_enabled_;
  StringPrefMember ssl_version_min_;
  StringPrefMember ssl_version_max_;
  StringListPrefMember h2_client_cert_coalescing_host_patterns_;

  // The cached list of disabled SSL cipher suites.
  std::vector<uint16_t> disabled_cipher_suites_;

  mojo::RemoteSet<network::mojom::SSLConfigClient> ssl_config_client_set_;

  DISALLOW_COPY_AND_ASSIGN(SSLConfigServiceManagerPref);
};

SSLConfigServiceManagerPref::SSLConfigServiceManagerPref(
    PrefService* local_state) {
  DCHECK(local_state);

  local_state->SetDefaultPrefValue(
      prefs::kTLS13HardeningForLocalAnchorsEnabled,
      base::Value(base::FeatureList::IsEnabled(
          features::kTLS13HardeningForLocalAnchors)));

  PrefChangeRegistrar::NamedChangeCallback local_state_callback =
      base::BindRepeating(&SSLConfigServiceManagerPref::OnPreferenceChanged,
                          base::Unretained(this), local_state);

  rev_checking_enabled_.Init(prefs::kCertRevocationCheckingEnabled, local_state,
                             local_state_callback);
  rev_checking_required_local_anchors_.Init(
      prefs::kCertRevocationCheckingRequiredLocalAnchors, local_state,
      local_state_callback);
  tls13_hardening_for_local_anchors_enabled_.Init(
      prefs::kTLS13HardeningForLocalAnchorsEnabled, local_state,
      local_state_callback);
  ssl_version_min_.Init(prefs::kSSLVersionMin, local_state,
                        local_state_callback);
  ssl_version_max_.Init(prefs::kSSLVersionMax, local_state,
                        local_state_callback);
  h2_client_cert_coalescing_host_patterns_.Init(
      prefs::kH2ClientCertCoalescingHosts, local_state, local_state_callback);

  local_state_change_registrar_.Init(local_state);
  local_state_change_registrar_.Add(prefs::kCipherSuiteBlacklist,
                                    local_state_callback);

  // Populate |disabled_cipher_suites_| with the initial pref value.
  OnDisabledCipherSuitesChange(local_state);
}

// static
void SSLConfigServiceManagerPref::RegisterPrefs(PrefRegistrySimple* registry) {
  net::CertVerifier::Config default_verifier_config;
  registry->RegisterBooleanPref(prefs::kCertRevocationCheckingEnabled,
                                default_verifier_config.enable_rev_checking);
  registry->RegisterBooleanPref(
      prefs::kCertRevocationCheckingRequiredLocalAnchors,
      default_verifier_config.require_rev_checking_local_anchors);
  net::SSLContextConfig default_context_config;
  registry->RegisterBooleanPref(
      prefs::kTLS13HardeningForLocalAnchorsEnabled,
      default_context_config.tls13_hardening_for_local_anchors_enabled);
  registry->RegisterStringPref(prefs::kSSLVersionMin, std::string());
  registry->RegisterStringPref(prefs::kSSLVersionMax, std::string());
  registry->RegisterListPref(prefs::kCipherSuiteBlacklist);
  registry->RegisterListPref(prefs::kH2ClientCertCoalescingHosts);
}

void SSLConfigServiceManagerPref::AddToNetworkContextParams(
    network::mojom::NetworkContextParams* network_context_params) {
  network_context_params->initial_ssl_config = GetSSLConfigFromPrefs();
  mojo::Remote<network::mojom::SSLConfigClient> ssl_config_client;
  network_context_params->ssl_config_client_receiver =
      ssl_config_client.BindNewPipeAndPassReceiver();
  ssl_config_client_set_.Add(std::move(ssl_config_client));
}

void SSLConfigServiceManagerPref::FlushForTesting() {
  ssl_config_client_set_.FlushForTesting();
}

void SSLConfigServiceManagerPref::OnPreferenceChanged(
    PrefService* prefs,
    const std::string& pref_name_in) {
  DCHECK(prefs);
  if (pref_name_in == prefs::kCipherSuiteBlacklist)
    OnDisabledCipherSuitesChange(prefs);

  network::mojom::SSLConfigPtr new_config = GetSSLConfigFromPrefs();
  network::mojom::SSLConfig* raw_config = new_config.get();

  for (const auto& client : ssl_config_client_set_) {
    // Mojo calls consume all InterfacePtrs passed to them, so have to
    // clone the config for each call.
    client->OnSSLConfigUpdated(raw_config->Clone());
  }
}

network::mojom::SSLConfigPtr
SSLConfigServiceManagerPref::GetSSLConfigFromPrefs() const {
  network::mojom::SSLConfigPtr config = network::mojom::SSLConfig::New();

  // rev_checking_enabled was formerly a user-settable preference, but now
  // it is managed-only.
  if (rev_checking_enabled_.IsManaged())
    config->rev_checking_enabled = rev_checking_enabled_.GetValue();
  else
    config->rev_checking_enabled = false;
  config->rev_checking_required_local_anchors =
      rev_checking_required_local_anchors_.GetValue();
  config->tls13_hardening_for_local_anchors_enabled =
      tls13_hardening_for_local_anchors_enabled_.GetValue();
  std::string version_min_str = ssl_version_min_.GetValue();
  std::string version_max_str = ssl_version_max_.GetValue();

  network::mojom::SSLVersion version_min;
  if (SSLProtocolVersionFromString(version_min_str, &version_min))
    config->version_min = version_min;

  network::mojom::SSLVersion version_max;
  if (SSLProtocolVersionFromString(version_max_str, &version_max) &&
      version_max >= network::mojom::SSLVersion::kTLS12) {
    config->version_max = version_max;
  }

  config->disabled_cipher_suites = disabled_cipher_suites_;
  config->client_cert_pooling_policy = CanonicalizeHostnamePatterns(
      h2_client_cert_coalescing_host_patterns_.GetValue());

  return config;
}

void SSLConfigServiceManagerPref::OnDisabledCipherSuitesChange(
    PrefService* local_state) {
  const base::ListValue* value =
      local_state->GetList(prefs::kCipherSuiteBlacklist);
  disabled_cipher_suites_ = ParseCipherSuites(ListValueToStringVector(value));
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
//  SSLConfigServiceManager

// static
SSLConfigServiceManager* SSLConfigServiceManager::CreateDefaultManager(
    PrefService* local_state) {
  return new SSLConfigServiceManagerPref(local_state);
}

// static
void SSLConfigServiceManager::RegisterPrefs(PrefRegistrySimple* registry) {
  SSLConfigServiceManagerPref::RegisterPrefs(registry);
}
