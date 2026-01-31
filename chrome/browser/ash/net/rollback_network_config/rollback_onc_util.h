// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_ROLLBACK_NETWORK_CONFIG_ROLLBACK_ONC_UTIL_H_
#define CHROME_BROWSER_ASH_NET_ROLLBACK_NETWORK_CONFIG_ROLLBACK_ONC_UTIL_H_

#include "base/values.h"

// Rollback-specific functions for managed and non-managed onc configurations
// of wifi and ethernet. These functions do not validate the onc format. To be
// used only with validated input, e.g. configurations coming from Chrome.
// Look for onc_spec.md for documentation of the onc format.
namespace ash::rollback_network_config {

// Returns the value of the given key. This will crash if string value can not
// be found.
std::string GetStringValue(const base::DictValue& network,
                           const std::string& key);

// Returns the value of the given key. This will crash if bool value can not be
// found.
bool GetBoolValue(const base::DictValue& network, const std::string& key);

// Managed ONC

// Collapses a managed onc dictionary to the values that are marked as active
// configuration. Values that are not split into managed dictionaries are kept.
void ManagedOncCollapseToActive(base::Value* network);

// Collapses a managed onc dictionary to the values that are marked as shared
// setting. Values that are not split into managed dictionaries are discarded.
// This results in the part that shill saves as UI data for the given managed
// network.
void ManagedOncCollapseToUiData(base::Value* network);

// Sets the PSK password of a managed onc dictionary as device or policy
// configured, depending on the source of the network.
void ManagedOncWiFiSetPskPassword(base::DictValue* network,
                                  const std::string& password);
// Sets the EAP password of a managed onc dictionary as device or policy
// configured, depending on the source of the network.
void ManagedOncSetEapPassword(base::DictValue* network,
                              const std::string& password);

// ONC

bool OncIsWiFi(const base::DictValue& network);
bool OncIsEthernet(const base::DictValue& network);

bool OncIsSourceDevicePolicy(const base::DictValue& network);
bool OncIsSourceDevice(const base::DictValue& network);

bool OncHasNoSecurity(const base::DictValue& network);

// Returns true if the given network is of a type that would require an EAP
// entry.
bool OncIsEap(const base::DictValue& network);

// Returns true if the given dictionary has an EAP entry.
bool OncHasEapConfiguration(const base::DictValue& network);

// Returns true if the given network is an EAP network and is of a type that
// does not require a client certificate.
bool OncIsEapWithoutClientCertificate(const base::DictValue& network);

// ONC>EAP
// The following functions only succeed if called on a dictionary containing
// an EAP entry within a wifi or ethernet entry.

std::string OncGetEapIdentity(const base::DictValue& network);
std::string OncGetEapInner(const base::DictValue& network);
std::string OncGetEapOuter(const base::DictValue& network);
bool OncGetEapSaveCredentials(const base::DictValue& network);
std::string OncGetEapPassword(const base::DictValue& network);
std::string OncGetEapClientCertType(const base::DictValue& network);
std::string OncGetEapClientCertPKCS11Id(const base::DictValue& network);

// Returns true if the given EAP wifi or ethernet is of a type that may require
// a client certificate.
bool OncEapRequiresClientCertificate(const base::DictValue& network);

void OncSetEapPassword(base::DictValue* network, const std::string& password);

// ONC>WiFi
// The following functions only succeed if called on a dictionary containing
// a wifi entry.

std::string OncWiFiGetSecurity(const base::DictValue& network);
std::string OncWiFiGetPassword(const base::DictValue& network);

bool OncWiFiIsPsk(const base::DictValue& network);

void OncWiFiSetPskPassword(base::DictValue* network,
                           const std::string& password);

// ONC>Ethernet
// The following functions only succeed if called on a dictionary containing
// an ethernet entry.

std::string OncEthernetGetAuthentication(const base::DictValue& network);

}  // namespace ash::rollback_network_config

#endif  // CHROME_BROWSER_ASH_NET_ROLLBACK_NETWORK_CONFIG_ROLLBACK_ONC_UTIL_H_
