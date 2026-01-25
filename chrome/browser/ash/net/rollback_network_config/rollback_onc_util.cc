// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/rollback_network_config/rollback_onc_util.h"

#include "base/check.h"
#include "base/notreached.h"
#include "base/values.h"

#include "components/onc/onc_constants.h"

namespace ash::rollback_network_config {

namespace {

static const char* const kAugmentationKeys[] = {
    onc::kAugmentationActiveSetting,  onc::kAugmentationEffectiveSetting,
    onc::kAugmentationUserPolicy,     onc::kAugmentationDevicePolicy,
    onc::kAugmentationUserSetting,    onc::kAugmentationSharedSetting,
    onc::kAugmentationUserEditable,   onc::kAugmentationDeviceEditable,
    onc::kAugmentationActiveExtension};

const base::DictValue* OncGetWiFi(const base::DictValue& network) {
  const base::DictValue* wifi = network.FindDict(onc::network_config::kWiFi);
  DCHECK(wifi);
  return wifi;
}

base::DictValue* OncGetWiFi(base::DictValue* network) {
  return const_cast<base::DictValue*>(OncGetWiFi(*network));
}

const base::DictValue* OncGetEthernet(const base::DictValue& network) {
  const base::DictValue* ethernet =
      network.FindDict(onc::network_config::kEthernet);
  DCHECK(ethernet);
  return ethernet;
}

const base::DictValue* OncGetEap(const base::DictValue& network) {
  if (OncIsWiFi(network)) {
    const base::DictValue* wifi = OncGetWiFi(network);
    const base::DictValue* eap = wifi->FindDict(onc::wifi::kEAP);
    DCHECK(eap);
    return eap;
  }
  if (OncIsEthernet(network)) {
    const base::DictValue* ethernet = OncGetEthernet(network);
    const base::DictValue* eap = ethernet->FindDict(onc::ethernet::kEAP);
    DCHECK(eap);
    return eap;
  }
  NOTREACHED();
}

base::DictValue* OncGetEap(base::DictValue* network) {
  return const_cast<base::DictValue*>(OncGetEap(*network));
}

base::DictValue ManagedOncCreatePasswordDict(const base::DictValue& network,
                                             const std::string& password) {
  std::string source = onc::kAugmentationDevicePolicy;
  if (OncIsSourceDevice(network)) {
    source = onc::kAugmentationSharedSetting;
  }

  return base::DictValue()
      .Set(onc::kAugmentationActiveSetting, password)
      .Set(onc::kAugmentationEffectiveSetting, source)
      .Set(source, password);
}

}  // namespace

std::string GetStringValue(const base::DictValue& network,
                           const std::string& key) {
  const std::string* value = network.FindString(key);
  DCHECK(value);
  return *value;
}

bool GetBoolValue(const base::DictValue& network, const std::string& key) {
  std::optional<bool> value = network.FindBool(key);
  DCHECK(value);
  return *value;
}

void ManagedOncCollapseToActive(base::Value* network) {
  DCHECK(network);
  base::DictValue* network_dict = network->GetIfDict();
  if (!network_dict) {
    return;
  }

  // FYI: don't fail to notice the fact that this out value assigned to
  // `network` might not be a dictionary. That is why the argument to this
  // function is not of `base::DictValue` type.
  base::Value* active = network_dict->Find(onc::kAugmentationActiveSetting);
  if (active) {
    *network = active->Clone();
    return;
  }

  std::vector<std::string> empty_dictionaries;
  for (const auto property : *network_dict) {
    ManagedOncCollapseToActive(&property.second);
    if (property.second.is_dict() && property.second.GetDict().empty()) {
      empty_dictionaries.push_back(property.first);
    }
  }
  for (const std::string& key : empty_dictionaries) {
    network_dict->Remove(key);
  }
  for (const std::string& key : kAugmentationKeys) {
    network_dict->Remove(key);
  }
}

void ManagedOncCollapseToUiData(base::Value* network) {
  DCHECK(network);
  base::DictValue& network_dict = network->GetDict();

  // FYI: don't fail to notice the fact that this out value assigned to
  // `network` might not be a dictionary. That is why the argument to this
  // function is not of `base::DictValue` type.
  base::Value* shared = network_dict.Find(onc::kAugmentationSharedSetting);
  if (shared) {
    *network = shared->Clone();
    return;
  }

  std::vector<std::string> to_remove;
  for (const auto property : network_dict) {
    if (!property.second.is_dict()) {
      to_remove.push_back(property.first);
    } else {
      // The call below may change the type of `property.second`, that's why we
      // need to check again (see above).
      ManagedOncCollapseToUiData(&property.second);
      base::DictValue* property_value_dict = property.second.GetIfDict();
      if (property_value_dict && property_value_dict->empty()) {
        to_remove.push_back(property.first);
      }
    }
  }
  for (const std::string& key : to_remove) {
    network_dict.Remove(key);
  }
}

void ManagedOncSetEapPassword(base::DictValue* network,
                              const std::string& password) {
  base::DictValue* eap = OncGetEap(network);
  eap->Set(onc::eap::kPassword,
           ManagedOncCreatePasswordDict(*network, password));
}

void ManagedOncWiFiSetPskPassword(base::DictValue* network,
                                  const std::string& password) {
  base::DictValue* wifi = OncGetWiFi(network);
  wifi->Set(onc::wifi::kPassphrase,
            ManagedOncCreatePasswordDict(*network, password));
}

bool OncIsWiFi(const base::DictValue& network) {
  return GetStringValue(network, onc::network_config::kType) ==
         onc::network_type::kWiFi;
}

bool OncIsEthernet(const base::DictValue& network) {
  return GetStringValue(network, onc::network_config::kType) ==
         onc::network_type::kEthernet;
}

bool OncIsSourceDevicePolicy(const base::DictValue& network) {
  return GetStringValue(network, onc::network_config::kSource) ==
         onc::network_config::kSourceDevicePolicy;
}

bool OncIsSourceDevice(const base::DictValue& network) {
  return GetStringValue(network, onc::network_config::kSource) ==
         onc::network_config::kSourceDevice;
}

bool OncHasNoSecurity(const base::DictValue& network) {
  if (OncIsWiFi(network)) {
    return OncWiFiGetSecurity(network) == onc::wifi::kSecurityNone;
  }
  if (OncIsEthernet(network)) {
    return OncEthernetGetAuthentication(network) ==
           onc::ethernet::kAuthenticationNone;
  }
  return false;
}

bool OncIsEap(const base::DictValue& network) {
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

bool OncHasEapConfiguration(const base::DictValue& network) {
  if (OncIsWiFi(network)) {
    const base::DictValue* wifi = OncGetWiFi(network);
    return wifi->FindDict(onc::wifi::kEAP);
  }
  if (OncIsEthernet(network)) {
    const base::DictValue* ethernet = OncGetEthernet(network);
    return ethernet->FindDict(onc::ethernet::kEAP);
  }
  return false;
}

bool OncIsEapWithoutClientCertificate(const base::DictValue& network) {
  return OncIsEap(network) && !OncEapRequiresClientCertificate(network);
}

std::string OncGetEapIdentity(const base::DictValue& network) {
  const base::DictValue* eap = OncGetEap(network);
  return GetStringValue(*eap, onc::eap::kIdentity);
}

std::string OncGetEapInner(const base::DictValue& network) {
  const base::DictValue* eap = OncGetEap(network);
  return GetStringValue(*eap, onc::eap::kInner);
}

std::string OncGetEapOuter(const base::DictValue& network) {
  const base::DictValue* eap = OncGetEap(network);
  return GetStringValue(*eap, onc::eap::kOuter);
}

bool OncGetEapSaveCredentials(const base::DictValue& network) {
  const base::DictValue* eap = OncGetEap(network);
  return GetBoolValue(*eap, onc::eap::kSaveCredentials);
}

std::string OncGetEapPassword(const base::DictValue& network) {
  const base::DictValue* eap = OncGetEap(network);
  return GetStringValue(*eap, onc::eap::kPassword);
}

std::string OncGetEapClientCertType(const base::DictValue& network) {
  const base::DictValue* eap = OncGetEap(network);
  return GetStringValue(*eap, onc::client_cert::kClientCertType);
}

std::string OncGetEapClientCertPKCS11Id(const base::DictValue& network) {
  const base::DictValue* eap = OncGetEap(network);
  return GetStringValue(*eap, onc::client_cert::kClientCertPKCS11Id);
}

bool OncEapRequiresClientCertificate(const base::DictValue& network) {
  // TODO(crbug/1225560) There may be unexpected client cert fields, so we
  // cannot rely on them. Simply check for EAP-TLS for now, which is the only
  // type for which a user may configure a client cert in the UI.
  return OncGetEapOuter(network) == onc::eap::kEAP_TLS;
}

void OncSetEapPassword(base::DictValue* network, const std::string& password) {
  base::DictValue* eap = OncGetEap(network);
  eap->Set(onc::eap::kPassword, password);
}

std::string OncWiFiGetSecurity(const base::DictValue& network) {
  const base::DictValue* wifi = OncGetWiFi(network);
  const std::string* security_type = wifi->FindString(onc::wifi::kSecurity);
  DCHECK(security_type);
  return *security_type;
}

std::string OncWiFiGetPassword(const base::DictValue& network) {
  const base::DictValue* wifi = OncGetWiFi(network);
  const std::string* password = wifi->FindString(onc::wifi::kPassphrase);
  DCHECK(password);
  return *password;
}

bool OncWiFiIsPsk(const base::DictValue& network) {
  const std::string security_type = OncWiFiGetSecurity(network);
  return security_type == onc::wifi::kWEP_PSK ||
         security_type == onc::wifi::kWPA_PSK ||
         security_type == onc::wifi::kWPA2_PSK;
}

void OncWiFiSetPskPassword(base::DictValue* network,
                           const std::string& password) {
  base::DictValue* wifi = OncGetWiFi(network);
  wifi->Set(onc::wifi::kPassphrase, password);
}

std::string OncEthernetGetAuthentication(const base::DictValue& network) {
  const std::string* type = network.FindDict(onc::network_config::kEthernet)
                                ->FindString(onc::ethernet::kAuthentication);
  DCHECK(type);
  return *type;
}

}  // namespace ash::rollback_network_config
