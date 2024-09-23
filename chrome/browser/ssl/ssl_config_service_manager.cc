// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ssl/ssl_config_service_manager.h"

#include <stdint.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/ssl_config.mojom.h"
#include "url/url_canon.h"

namespace {

// Converts a `base::Value::List` of StringValues into a vector of strings. Any
// values which cannot be converted will be skipped.
std::vector<std::string> ValueListToStringVector(
    const base::Value::List& list) {
  std::vector<std::string> results;
  results.reserve(list.size());
  for (const auto& entry : list) {
    const std::string* s = entry.GetIfString();
    if (s)
      results.push_back(*s);
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
  for (std::string_view pattern : patterns) {
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

}  // namespace

SSLConfigServiceManager::SSLConfigServiceManager(PrefService* local_state) {
  DCHECK(local_state);

  PrefChangeRegistrar::NamedChangeCallback local_state_callback =
      base::BindRepeating(&SSLConfigServiceManager::OnPreferenceChanged,
                          base::Unretained(this), local_state);

  rev_checking_enabled_.Init(prefs::kCertRevocationCheckingEnabled, local_state,
                             local_state_callback);
  rev_checking_required_local_anchors_.Init(
      prefs::kCertRevocationCheckingRequiredLocalAnchors, local_state,
      local_state_callback);
  ssl_version_min_.Init(prefs::kSSLVersionMin, local_state,
                        local_state_callback);
  ssl_version_max_.Init(prefs::kSSLVersionMax, local_state,
                        local_state_callback);
  h2_client_cert_coalescing_host_patterns_.Init(
      prefs::kH2ClientCertCoalescingHosts, local_state, local_state_callback);
  post_quantum_enabled_.Init(prefs::kPostQuantumKeyAgreementEnabled,
                             local_state, local_state_callback);
#if BUILDFLAG(IS_CHROMEOS)
  device_post_quantum_enabled_.Init(
      prefs::kDevicePostQuantumKeyAgreementEnabled, local_state,
      local_state_callback);
#endif
  ech_enabled_.Init(prefs::kEncryptedClientHelloEnabled, local_state,
                    local_state_callback);

  local_state_change_registrar_.Init(local_state);
  local_state_change_registrar_.Add(prefs::kCipherSuiteBlacklist,
                                    local_state_callback);

  // Populate |disabled_cipher_suites_| with the initial pref value.
  OnDisabledCipherSuitesChange(local_state);
}

SSLConfigServiceManager::~SSLConfigServiceManager() = default;

// static
void SSLConfigServiceManager::RegisterPrefs(PrefRegistrySimple* registry) {
  net::CertVerifier::Config default_verifier_config;
  registry->RegisterBooleanPref(prefs::kCertRevocationCheckingEnabled,
                                default_verifier_config.enable_rev_checking);
  registry->RegisterBooleanPref(
      prefs::kCertRevocationCheckingRequiredLocalAnchors,
      default_verifier_config.require_rev_checking_local_anchors);
  net::SSLContextConfig default_context_config;
  registry->RegisterStringPref(prefs::kSSLVersionMin, std::string());
  registry->RegisterStringPref(prefs::kSSLVersionMax, std::string());
  registry->RegisterListPref(prefs::kCipherSuiteBlacklist);
  registry->RegisterListPref(prefs::kH2ClientCertCoalescingHosts);
  registry->RegisterBooleanPref(prefs::kEncryptedClientHelloEnabled,
                                default_context_config.ech_enabled);

  // Default value for these prefs don't matter since they are only used when
  // managed.
  registry->RegisterBooleanPref(prefs::kPostQuantumKeyAgreementEnabled, false);
#if BUILDFLAG(IS_CHROMEOS)
  registry->RegisterBooleanPref(prefs::kDevicePostQuantumKeyAgreementEnabled,
                                true);
#endif
}

void SSLConfigServiceManager::AddToNetworkContextParams(
    network::mojom::NetworkContextParams* network_context_params) {
  network_context_params->initial_ssl_config = GetSSLConfigFromPrefs();
  mojo::Remote<network::mojom::SSLConfigClient> ssl_config_client;
  network_context_params->ssl_config_client_receiver =
      ssl_config_client.BindNewPipeAndPassReceiver();
  ssl_config_client_set_.Add(std::move(ssl_config_client));
}

void SSLConfigServiceManager::FlushForTesting() {
  ssl_config_client_set_.FlushForTesting();
}

void SSLConfigServiceManager::OnPreferenceChanged(
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

network::mojom::SSLConfigPtr SSLConfigServiceManager::GetSSLConfigFromPrefs()
    const {
  network::mojom::SSLConfigPtr config = network::mojom::SSLConfig::New();

  // rev_checking_enabled was formerly a user-settable preference, but now
  // it is managed-only.
  if (rev_checking_enabled_.IsManaged())
    config->rev_checking_enabled = rev_checking_enabled_.GetValue();
  else
    config->rev_checking_enabled = false;
  config->rev_checking_required_local_anchors =
      rev_checking_required_local_anchors_.GetValue();
  std::string version_min_str = ssl_version_min_.GetValue();
  std::string version_max_str = ssl_version_max_.GetValue();

  network::mojom::SSLVersion version_min;
  if (SSLProtocolVersionFromString(version_min_str, &version_min)) {
    config->version_min = version_min;
  }

  network::mojom::SSLVersion version_max;
  if (SSLProtocolVersionFromString(version_max_str, &version_max)) {
    config->version_max = version_max;
  }

  config->disabled_cipher_suites = disabled_cipher_suites_;
  config->client_cert_pooling_policy = CanonicalizeHostnamePatterns(
      h2_client_cert_coalescing_host_patterns_.GetValue());

  config->ech_enabled = ech_enabled_.GetValue();

  config->post_quantum_override = network::mojom::OptionalBool::kUnset;
  if (post_quantum_enabled_.IsManaged()) {
    config->post_quantum_override = post_quantum_enabled_.GetValue()
                                        ? network::mojom::OptionalBool::kTrue
                                        : network::mojom::OptionalBool::kFalse;
  }
#if BUILDFLAG(IS_CHROMEOS)
  if (device_post_quantum_enabled_.IsManaged()) {
    config->post_quantum_override = device_post_quantum_enabled_.GetValue()
                                        ? network::mojom::OptionalBool::kTrue
                                        : network::mojom::OptionalBool::kFalse;
  }
#endif

  return config;
}

void SSLConfigServiceManager::OnDisabledCipherSuitesChange(
    PrefService* local_state) {
  const base::Value::List& list =
      local_state->GetList(prefs::kCipherSuiteBlacklist);
  disabled_cipher_suites_ = ParseCipherSuites(ValueListToStringVector(list));
}
