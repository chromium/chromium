// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/attestation_utils.h"

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "chrome/browser/enterprise/connectors/device_trust/attestation/common/signals_type.h"
#include "components/device_signals/core/common/signals_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {

std::string ProtobufChallengeToJsonChallenge(
    const std::string& challenge_response) {
  base::Value signed_data(base::Value::Type::DICTIONARY);

  std::string encoded;
  base::Base64Encode(challenge_response, &encoded);

  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetKey("challengeResponse", base::Value(encoded));

  std::string json;
  base::JSONWriter::Write(dict, &json);
  return json;
}

std::unique_ptr<SignalsType> DictionarySignalsToProtobufSignals(
    const base::Value::Dict& signals_dict) {
  auto signals_proto = std::make_unique<SignalsType>();
  if (signals_dict.empty()) {
    return signals_proto;
  }

  const absl::optional<bool> allow_screen_lock =
      signals_dict.FindBool(device_signals::names::kAllowScreenLock);
  if (allow_screen_lock) {
    signals_proto->set_allow_screen_lock(allow_screen_lock.value());
  }
  const std::string* browser_version =
      signals_dict.FindString(device_signals::names::kBrowserVersion);
  if (browser_version) {
    signals_proto->set_browser_version(*browser_version);
  }

  const absl::optional<bool> dns_client_enabled =
      signals_dict.FindBool(device_signals::names::kBuiltInDnsClientEnabled);
  if (dns_client_enabled) {
    signals_proto->set_built_in_dns_client_enabled(dns_client_enabled.value());
  }

  const absl::optional<bool> chrome_cleanup_enabled =
      signals_dict.FindBool(device_signals::names::kChromeCleanupEnabled);
  if (chrome_cleanup_enabled) {
    signals_proto->set_chrome_cleanup_enabled(chrome_cleanup_enabled.value());
  }

  const std::string* device_id =
      signals_dict.FindString(device_signals::names::kDeviceId);
  if (device_id) {
    signals_proto->set_device_id(*device_id);
  }

  const std::string* device_manufacturer =
      signals_dict.FindString(device_signals::names::kDeviceManufacturer);
  if (device_manufacturer) {
    signals_proto->set_device_manufacturer(*device_manufacturer);
  }

  const std::string* device_model =
      signals_dict.FindString(device_signals::names::kDeviceModel);
  if (device_model) {
    signals_proto->set_device_model(*device_model);
  }

  const std::string* display_name =
      signals_dict.FindString(device_signals::names::kDisplayName);
  if (display_name) {
    signals_proto->set_display_name(*display_name);
  }

  const std::string* dns_address =
      signals_dict.FindString(device_signals::names::kDnsAddress);
  if (dns_address) {
    signals_proto->set_dns_address(*dns_address);
  }

  const std::string* enrollement_domain =
      signals_dict.FindString(device_signals::names::kEnrollmentDomain);
  if (enrollement_domain) {
    signals_proto->set_enrollment_domain(*enrollement_domain);
  }

  const absl::optional<bool> firewall_on =
      signals_dict.FindBool(device_signals::names::kFirewallOn);
  if (firewall_on) {
    signals_proto->set_firewall_on(firewall_on.value());
  }

  const base::Value::List* imei_list =
      signals_dict.FindList(device_signals::names::kImei);
  if (imei_list) {
    for (const auto& imei : *imei_list) {
      if (imei.is_string()) {
        signals_proto->add_imei(imei.GetString());
      }
    }
  }

  const absl::optional<bool> is_disk_encrypted =
      signals_dict.FindBool(device_signals::names::kIsDiskEncrypted);
  if (is_disk_encrypted) {
    signals_proto->set_is_disk_encrypted(is_disk_encrypted.value());
  }

  const absl::optional<bool> is_jailbroken =
      signals_dict.FindBool(device_signals::names::kIsJailbroken);
  if (is_jailbroken) {
    signals_proto->set_is_jailbroken(is_jailbroken.value());
  }

  const absl::optional<bool> is_password_protected =
      signals_dict.FindBool(device_signals::names::kIsPasswordProtected);
  if (is_password_protected) {
    signals_proto->set_is_protected_by_password(is_password_protected.value());
  }

  const base::Value::List* meid_list =
      signals_dict.FindList(device_signals::names::kMeid);
  if (meid_list) {
    for (const auto& meid : *meid_list) {
      if (meid.is_string()) {
        signals_proto->add_meid(meid.GetString());
      }
    }
  }

  const std::string* obfuscated_customer_id =
      signals_dict.FindString(device_signals::names::kObfuscatedCustomerId);
  if (obfuscated_customer_id) {
    signals_proto->set_obfuscated_customer_id(*obfuscated_customer_id);
  }

  const std::string* os = signals_dict.FindString(device_signals::names::kOs);
  if (os) {
    signals_proto->set_os(*os);
  }

  const std::string* os_version =
      signals_dict.FindString(device_signals::names::kOsVersion);
  if (os_version) {
    signals_proto->set_os_version(*os_version);
  }

  const absl::optional<int> password_protection_warning_trigger =
      signals_dict.FindInt(
          device_signals::names::kPasswordProtectionWarningTrigger);
  if (password_protection_warning_trigger) {
    signals_proto->set_password_protection_warning_trigger(
        password_protection_warning_trigger.value());
  }

  const absl::optional<bool> remote_desktop_available =
      signals_dict.FindBool(device_signals::names::kRemoteDesktopAvailable);
  if (remote_desktop_available) {
    signals_proto->set_remote_desktop_available(
        remote_desktop_available.value());
  }

  const absl::optional<int> safe_browsing_protection_level =
      signals_dict.FindInt(device_signals::names::kSafeBrowsingProtectionLevel);
  if (safe_browsing_protection_level) {
    signals_proto->set_safe_browsing_protection_level(
        safe_browsing_protection_level.value());
  }

  const std::string* serial_number =
      signals_dict.FindString(device_signals::names::kSerialNumber);
  if (serial_number) {
    signals_proto->set_serial_number(*serial_number);
  }

  const std::string* profile_name =
      signals_dict.FindString(device_signals::names::kSignedInProfileName);
  if (profile_name) {
    signals_proto->set_signed_in_profile_name(*profile_name);
  }

  const absl::optional<bool> site_isolation_enabled =
      signals_dict.FindBool(device_signals::names::kSiteIsolationEnabled);
  if (site_isolation_enabled) {
    signals_proto->set_site_isolation_enabled(site_isolation_enabled.value());
  }

  const absl::optional<bool> third_party_blocking_enabled =
      signals_dict.FindBool(device_signals::names::kThirdPartyBlockingEnabled);
  if (third_party_blocking_enabled) {
    signals_proto->set_third_party_blocking_enabled(
        third_party_blocking_enabled.value());
  }

  const std::string* tpm_hash =
      signals_dict.FindString(device_signals::names::kTpmHash);
  if (tpm_hash) {
    signals_proto->set_tpm_hash(*tpm_hash);
  }

  const std::string* windows_domain =
      signals_dict.FindString(device_signals::names::kWindowsDomain);
  if (windows_domain) {
    signals_proto->set_windows_domain(*windows_domain);
  }

  return signals_proto;
}

}  // namespace enterprise_connectors
