// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/rollback_network_config/rollback_onc_util.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/values.h"

#include "components/onc/onc_constants.h"

namespace ash {
namespace rollback_network_config {

namespace {

static const char* const kAugmentationKeys[] = {
    onc::kAugmentationActiveSetting,  onc::kAugmentationEffectiveSetting,
    onc::kAugmentationUserPolicy,     onc::kAugmentationDevicePolicy,
    onc::kAugmentationUserSetting,    onc::kAugmentationSharedSetting,
    onc::kAugmentationUserEditable,   onc::kAugmentationDeviceEditable,
    onc::kAugmentationActiveExtension};

const base::Value* OncGetWiFi(const base::Value& network) {
  const base::Value* wifi = network.FindDictKey(onc::network_config::kWiFi);
  DCHECK(wifi);
  return wifi;
}

base::Value* OncGetWiFi(base::Value* network) {
  return const_cast<base::Value*>(OncGetWiFi(*network));
}

const base::Value* OncGetEthernet(const base::Value& network) {
  const base::Value* ethernet =
      network.FindDictKey(onc::network_config::kEthernet);
  DCHECK(ethernet);
  return ethernet;
}

const base::Value* OncGetEap(const base::Value& network) {
  if (OncIsWiFi(network)) {
    const base::Value* wifi = OncGetWiFi(network);
    const base::Value* eap = wifi->FindDictKey(onc::wifi::kEAP);
    DCHECK(eap);
    return eap;
  }
  if (OncIsEthernet(network)) {
    const base::Value* ethernet = OncGetEthernet(network);
    const base::Value* eap = ethernet->FindDictKey(onc::ethernet::kEAP);
    DCHECK(eap);
    return eap;
  }
  NOTREACHED();
  return nullptr;
}

base::Value* OncGetEap(base::Value* network) {
  return const_cast<base::Value*>(OncGetEap(*network));
}

base::Value ManagedOncCreatePasswordDict(const base::Value& network,
                                         const std::string& password) {
  std::string source = onc::kAugmentationDevicePolicy;
  if (OncIsSourceDevice(network)) {
    source = onc::kAugmentationSharedSetting;
  }

  base::Value password_dict(base::Value::Type::DICTIONARY);
  password_dict.SetStringKey(onc::kAugmentationActiveSetting, password);
  password_dict.SetStringKey(onc::kAugmentationEffectiveSetting, source);
  password_dict.SetStringKey(source, password);

  return password_dict;
}

}  // namespace

std::string GetStringValue(const base::Value& network, const std::string& key) {
  const std::string* value = network.FindStringKey(key);
  DCHECK(value);
  return *value;
}

bool GetBoolValue(const base::Value& network, const std::string& key) {
  absl::optional<bool> value = network.FindBoolKey(key);
  DCHECK(value);
  return *value;
}

void ManagedOncCollapseToActive(base::Value* network) {
  DCHECK(network);
  if (!network->is_dict()) {
    return;
  }

  base::Value* active = network->FindKey(onc::kAugmentationActiveSetting);
  if (active) {
    *network = active->Clone();
    return;
  }

  std::vector<std::string> empty_dictionaries;
  for (const auto property : network->DictItems()) {
    ManagedOncCollapseToActive(&property.second);
    if (property.second.is_dict() && property.second.DictEmpty()) {
      empty_dictionaries.push_back(property.first);
    }
  }
  for (const std::string& key : empty_dictionaries) {
    network->RemoveKey(key);
  }
  for (const std::string& key : kAugmentationKeys) {
    network->RemoveKey(key);
  }
}

void ManagedOncCollapseToUiData(base::Value* network) {
  DCHECK(network);
  DCHECK(network->is_dict());

  base::Value* shared = network->FindKey(onc::kAugmentationSharedSetting);
  if (shared) {
    *network = shared->Clone();
    return;
  }

  std::vector<std::string> to_remove;
  for (const auto property : network->DictItems()) {
    if (!property.second.is_dict()) {
      to_remove.push_back(property.first);
    } else {
      // The call below may change the type of |property.second|,
      // that's why we need to check again.
      ManagedOncCollapseToUiData(&property.second);
      if (property.second.is_dict() && property.second.DictEmpty()) {
        to_remove.push_back(property.first);
      }
    }
  }
  for (const std::string& key : to_remove) {
    network->RemoveKey(key);
  }
}

void ManagedOncSetEapPassword(base::Value* network,
                              const std::string& password) {
  base::Value* eap = OncGetEap(network);
  eap->SetKey(onc::eap::kPassword,
              ManagedOncCreatePasswordDict(*network, password));
}

void ManagedOncWiFiSetPskPassword(base::Value* network,
                                  const std::string& password) {
  base::Value* wifi = OncGetWiFi(network);
  wifi->SetKey(onc::wifi::kPassphrase,
               ManagedOncCreatePasswordDict(*network, password));
}

bool OncIsWiFi(const base::Value& network) {
  return GetStringValue(network, onc::network_config::kType) ==
         onc::network_type::kWiFi;
}

bool OncIsEthernet(const base::Value& network) {
  return GetStringValue(network, onc::network_config::kType) ==
         onc::network_type::kEthernet;
}

bool OncIsSourceDevicePolicy(const base::Value& network) {
  return GetStringValue(network, onc::network_config::kSource) ==
         onc::network_config::kSourceDevicePolicy;
}

bool OncIsSourceDevice(const base::Value& network) {
  return GetStringValue(network, onc::network_config::kSource) ==
         onc::network_config::kSourceDevice;
}

bool OncHasNoSecurity(const base::Value& network) {
  if (OncIsWiFi(network)) {
    return OncWiFiGetSecurity(network) == onc::wifi::kSecurityNone;
  }
  if (OncIsEthernet(network)) {
    return OncEthernetGetAuthentication(network) ==
           onc::ethernet::kAuthenticationNone;
  }
  return false;
}

bool OncIsEap(const base::Value& network) {
  if (OncIsWiFi(network)) {
    const std::string security_type = OncWiFiGetSecurity(network);
    return security_type == onc::wifi::kWEP_8021X ||
           security_type == onc::wifi::kWPA_EAP;
  }
  if (OncIsEthernet(network)) {
    const std::string authentication_type =
        OncEthernetGetAuthentication(network);
    return authentication_type == onc::ethernet::k8021X;
  }
  return false;
}

bool OncHasEapConfiguration(const base::Value& network) {
  if (OncIsWiFi(network)) {
    const base::Value* wifi = OncGetWiFi(network);
    return wifi->FindDictKey(onc::wifi::kEAP);
  }
  if (OncIsEthernet(network)) {
    const base::Value* ethernet = OncGetEthernet(network);
    return ethernet->FindDictKey(onc::ethernet::kEAP);
  }
  return false;
}

bool OncIsEapWithoutClientCertificate(const base::Value& network) {
  return OncIsEap(network) && !OncEapRequiresClientCertificate(network);
}

std::string OncGetEapIdentity(const base::Value& network) {
  const base::Value* eap = OncGetEap(network);
  return GetStringValue(*eap, onc::eap::kIdentity);
}

std::string OncGetEapInner(const base::Value& network) {
  const base::Value* eap = OncGetEap(network);
  return GetStringValue(*eap, onc::eap::kInner);
}

std::string OncGetEapOuter(const base::Value& network) {
  const base::Value* eap = OncGetEap(network);
  return GetStringValue(*eap, onc::eap::kOuter);
}

bool OncGetEapSaveCredentials(const base::Value& network) {
  const base::Value* eap = OncGetEap(network);
  return GetBoolValue(*eap, onc::eap::kSaveCredentials);
}

std::string OncGetEapPassword(const base::Value& network) {
  const base::Value* eap = OncGetEap(network);
  return GetStringValue(*eap, onc::eap::kPassword);
}

std::string OncGetEapClientCertType(const base::Value& network) {
  const base::Value* eap = OncGetEap(network);
  return GetStringValue(*eap, onc::client_cert::kClientCertType);
}

std::string OncGetEapClientCertPKCS11Id(const base::Value& network) {
  const base::Value* eap = OncGetEap(network);
  return GetStringValue(*eap, onc::client_cert::kClientCertPKCS11Id);
}

bool OncEapRequiresClientCertificate(const base::Value& network) {
  // TODO(crbug/1225560) There may be unexpected client cert fields, so we
  // cannot rely on them. Simply check for EAP-TLS for now, which is the only
  // type for which a user may configure a client cert in the UI.
  return OncGetEapOuter(network) == onc::eap::kEAP_TLS;
}

void OncSetEapPassword(base::Value* network, const std::string& password) {
  base::Value* eap = OncGetEap(network);
  eap->SetStringKey(onc::eap::kPassword, password);
}

std::string OncWiFiGetSecurity(const base::Value& network) {
  const base::Value* wifi = OncGetWiFi(network);
  const std::string* security_type = wifi->FindStringKey(onc::wifi::kSecurity);
  DCHECK(security_type);
  return *security_type;
}

std::string OncWiFiGetPassword(const base::Value& network) {
  const base::Value* wifi = OncGetWiFi(network);
  const std::string* password = wifi->FindStringKey(onc::wifi::kPassphrase);
  DCHECK(password);
  return *password;
}

bool OncWiFiIsPsk(const base::Value& network) {
  const std::string security_type = OncWiFiGetSecurity(network);
  return security_type == onc::wifi::kWEP_PSK ||
         security_type == onc::wifi::kWPA_PSK ||
         security_type == onc::wifi::kWPA2_PSK;
}

void OncWiFiSetPskPassword(base::Value* network, const std::string& password) {
  base::Value* wifi = OncGetWiFi(network);
  wifi->SetStringKey(onc::wifi::kPassphrase, password);
}

std::string OncEthernetGetAuthentication(const base::Value& network) {
  const std::string* type = network.FindDictKey(onc::network_config::kEthernet)
                                ->FindStringKey(onc::ethernet::kAuthentication);
  DCHECK(type);
  return *type;
}

}  // namespace rollback_network_config
}  // namespace ash
