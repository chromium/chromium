// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/platform_apps/api/music_manager_private/device_id.h"

#include <stdint.h>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/hmac.h"

namespace chrome_apps {
namespace api {

namespace {

// Compute HMAC-SHA256(|key|, |text|) as a string.
bool ComputeHmacSha256(const std::string& key,
                       const std::string& text,
                       std::string* signature_return) {
  crypto::HMAC hmac(crypto::HMAC::SHA256);
  const size_t digest_length = hmac.DigestLength();
  std::vector<uint8_t> digest(digest_length);
  bool result = hmac.Init(key) && hmac.Sign(text, &digest[0], digest.size());
  if (result) {
    *signature_return =
        base::ToLowerASCII(base::HexEncode(digest.data(), digest.size()));
  }
  return result;
}

void GetRawDeviceIdCallback(const std::string& extension_id,
                            const DeviceId::IdCallback& callback,
                            const std::string& raw_device_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (raw_device_id.empty()) {
    callback.Run("");
    return;
  }

  std::string device_id;
  if (!ComputeHmacSha256(raw_device_id, extension_id, &device_id)) {
    DLOG(ERROR) << "Error while computing HMAC-SHA256 of device id.";
    callback.Run("");
    return;
  }
  callback.Run(device_id);
}

bool IsValidMacAddressImpl(const void* bytes, size_t size) {
  const size_t MAC_LENGTH = 6;
  const size_t OUI_LENGTH = 3;
  struct InvalidMacEntry {
    size_t size;
    unsigned char address[MAC_LENGTH];
  };

  // VPN, virtualization, tethering, bluetooth, etc.
  static InvalidMacEntry invalidAddresses[] = {
      // Empty address
      {MAC_LENGTH, {0, 0, 0, 0, 0, 0}},
      // VMware
      {OUI_LENGTH, {0x00, 0x50, 0x56}},
      {OUI_LENGTH, {0x00, 0x05, 0x69}},
      {OUI_LENGTH, {0x00, 0x0c, 0x29}},
      {OUI_LENGTH, {0x00, 0x1c, 0x14}},
      // VirtualBox
      {OUI_LENGTH, {0x08, 0x00, 0x27}},
      // PdaNet
      {MAC_LENGTH, {0x00, 0x26, 0x37, 0xbd, 0x39, 0x42}},
      // Cisco AnyConnect VPN
      {MAC_LENGTH, {0x00, 0x05, 0x9a, 0x3c, 0x7a, 0x00}},
      // Marvell sometimes uses this as a dummy address
      {MAC_LENGTH, {0x00, 0x11, 0x22, 0x33, 0x44, 0x55}},
      // Apple uses this across machines for Bluetooth ethernet adapters.
      {MAC_LENGTH - 1, {0x65, 0x90, 0x07, 0x42, 0xf1}},
      // Juniper uses this for their Virtual Adapter, the other 4 bytes are
      // reassigned at every boot. 00-ff-xx is not assigned to anyone.
      {2, {0x00, 0xff}},
      // T-Mobile Wireless Ethernet
      {MAC_LENGTH, {0x00, 0xa0, 0xc6, 0x00, 0x00, 0x00}},
      // Generic Bluetooth device
      {MAC_LENGTH, {0x00, 0x15, 0x83, 0x3d, 0x0a, 0x57}},
      // RAS Async Adapter
      {MAC_LENGTH, {0x20, 0x41, 0x53, 0x59, 0x4e, 0xff}},
      // Qualcomm USB ethernet adapter
      {MAC_LENGTH, {0x00, 0xa0, 0xc6, 0x00, 0x00, 0x00}},
      // Windows VPN
      {MAC_LENGTH, {0x00, 0x53, 0x45, 0x00, 0x00, 0x00}},
      // Bluetooth
      {MAC_LENGTH, {0x00, 0x1f, 0x81, 0x00, 0x08, 0x30}},
      {MAC_LENGTH, {0x00, 0x1b, 0x10, 0x00, 0x2a, 0xec}},
      {MAC_LENGTH, {0x00, 0x15, 0x83, 0x15, 0xa3, 0x10}},
      {MAC_LENGTH, {0x00, 0x15, 0x83, 0x07, 0xC6, 0x5A}},
      {MAC_LENGTH, {0x00, 0x1f, 0x81, 0x00, 0x02, 0x00}},
      {MAC_LENGTH, {0x00, 0x1f, 0x81, 0x00, 0x02, 0xdd}},
      // Ceton TV tuner
      {MAC_LENGTH, {0x00, 0x22, 0x2c, 0xff, 0xff, 0xff}},
      // Check Point VPN
      {MAC_LENGTH, {0x54, 0x55, 0x43, 0x44, 0x52, 0x09}},
      {MAC_LENGTH, {0x54, 0xEF, 0x14, 0x71, 0xE4, 0x0E}},
      {MAC_LENGTH, {0x54, 0xBA, 0xC6, 0xFF, 0x74, 0x10}},
      // Cisco VPN
      {MAC_LENGTH, {0x00, 0x05, 0x9a, 0x3c, 0x7a, 0x00}},
      // Cisco VPN
      {MAC_LENGTH, {0x00, 0x05, 0x9a, 0x3c, 0x78, 0x00}},
      // Intel USB cell modem
      {MAC_LENGTH, {0x00, 0x1e, 0x10, 0x1f, 0x00, 0x01}},
      // Microsoft tethering
      {MAC_LENGTH, {0x80, 0x00, 0x60, 0x0f, 0xe8, 0x00}},
      // Nortel VPN
      {MAC_LENGTH, {0x44, 0x45, 0x53, 0x54, 0x42, 0x00}},
      // AEP VPN
      {MAC_LENGTH, {0x00, 0x30, 0x70, 0x00, 0x00, 0x01}},
      // Positive VPN
      {MAC_LENGTH, {0x00, 0x02, 0x03, 0x04, 0x05, 0x06}},
      // Bluetooth
      {MAC_LENGTH, {0x00, 0x15, 0x83, 0x0B, 0x13, 0xC0}},
      // Kerio Virtual Network Adapter
      {MAC_LENGTH, {0x44, 0x45, 0x53, 0x54, 0x4f, 0x53}},
      // Sierra Wireless cell modems.
      {OUI_LENGTH, {0x00, 0xA0, 0xD5}},
      // FRITZ!web DSL
      {MAC_LENGTH, {0x00, 0x04, 0x0E, 0xFF, 0xFF, 0xFF}},
      // VirtualPC
      {MAC_LENGTH, {0x00, 0x00, 0x00, 0x00, 0x00, 0x01}},
      // Bluetooth
      {MAC_LENGTH, {0x00, 0x1F, 0x81, 0x00, 0x01, 0x00}},
      {MAC_LENGTH, {0x00, 0x30, 0x91, 0x10, 0x00, 0x26}},
      {MAC_LENGTH, {0x00, 0x25, 0x00, 0x5A, 0xC3, 0xD0}},
      {MAC_LENGTH, {0x00, 0x15, 0x83, 0x0C, 0xBF, 0xEB}},
      // Huawei cell modem
      {MAC_LENGTH, {0x58, 0x2C, 0x80, 0x13, 0x92, 0x63}},
      // Fortinet VPN
      {OUI_LENGTH, {0x00, 0x09, 0x0F}},
      // Realtek
      {MAC_LENGTH, {0x00, 0x00, 0x00, 0x00, 0x00, 0x30}},
      // Other rare dupes.
      {MAC_LENGTH, {0x00, 0x11, 0xf5, 0x0d, 0x8a, 0xe8}},  // Atheros
      {MAC_LENGTH, {0x00, 0x20, 0x07, 0x01, 0x16, 0x06}},  // Atheros
      {MAC_LENGTH, {0x0d, 0x0b, 0x00, 0x00, 0xe0, 0x00}},  // Atheros
      {MAC_LENGTH, {0x90, 0x4c, 0xe5, 0x0b, 0xc8, 0x8e}},  // Atheros
      {MAC_LENGTH, {0x00, 0x1c, 0x23, 0x38, 0x49, 0xa4}},  // Broadcom
      {MAC_LENGTH, {0x00, 0x12, 0x3f, 0x82, 0x7c, 0x32}},  // Broadcom
      {MAC_LENGTH, {0x00, 0x11, 0x11, 0x32, 0xc3, 0x77}},  // Broadcom
      {MAC_LENGTH, {0x00, 0x24, 0xd6, 0xae, 0x3e, 0x39}},  // Microsoft
      {MAC_LENGTH, {0x00, 0x0f, 0xb0, 0x3a, 0xb4, 0x80}},  // Realtek
      {MAC_LENGTH, {0x08, 0x10, 0x74, 0xa1, 0xda, 0x1b}},  // Realtek
      {MAC_LENGTH, {0x00, 0x21, 0x9b, 0x2a, 0x0a, 0x9c}},  // Realtek
  };

  if (size != MAC_LENGTH) {
    return false;
  }

  if (static_cast<const unsigned char*>(bytes)[0] & 0x02) {
    // Locally administered.
    return false;
  }

  for (size_t i = 0; i < arraysize(invalidAddresses); ++i) {
    size_t count = invalidAddresses[i].size;
    if (memcmp(invalidAddresses[i].address, bytes, count) == 0) {
      return false;
    }
  }
  return true;
}

}  // namespace

// static
void DeviceId::GetDeviceId(const std::string& extension_id,
                           const IdCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  CHECK(!extension_id.empty());

  // Forward call to platform specific implementation, then compute the HMAC
  // in the callback.
  GetRawDeviceId(base::Bind(&GetRawDeviceIdCallback, extension_id, callback));
}

// static
bool DeviceId::IsValidMacAddress(const void* bytes, size_t size) {
  return IsValidMacAddressImpl(bytes, size);
}

}  // namespace api
}  // namespace chrome_apps
