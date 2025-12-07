// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_DEVICE_H_
#define ASH_QUICK_PAIR_COMMON_DEVICE_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "ash/quick_pair/common/protocol.h"
#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"

namespace ash {
namespace quick_pair {

enum class DeviceFastPairVersion {
  kV1,
  kHigherThanV1,
};

// Thin class which is used by the higher level components of the Quick Pair
// system to represent a device.
//
// Lower level components will use |protocol|, |metadata_id|, |ble_address| and
// |account_key| to fetch objects which contain more information. E.g. A
// Fast Pair component can use |metadata_id| to query the Service to receive a
// full metadata object.
class COMPONENT_EXPORT(QUICK_PAIR_COMMON) Device
    : public base::RefCounted<Device> {
 public:
  Device(const std::string& metadata_id,
         const std::string& ble_address,
         Protocol protocol);
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;
  Device& operator=(Device&&) = delete;

  const std::optional<std::string>& classic_address() const {
    return classic_address_;
  }

  void set_classic_address(const std::optional<std::string>& address) {
    classic_address_ = address;
  }

  const std::optional<std::string>& display_name() const {
    return display_name_;
  }

  void set_display_name(const std::optional<std::string>& display_name) {
    display_name_ = display_name;
  }

  const std::optional<DeviceFastPairVersion> version() const {
    return version_;
  }

  void set_version(std::optional<DeviceFastPairVersion> version) {
    version_ = version;
  }

  const std::optional<std::vector<uint8_t>> account_key() const {
    return account_key_;
  }

  void set_account_key(const std::vector<uint8_t>& account_key) {
    account_key_ = account_key;
  }

  const std::optional<uint8_t> key_based_pairing_flags() const {
    return key_based_pairing_flags_;
  }

  void set_key_based_pairing_flags(uint8_t key_based_pairing_flags) {
    key_based_pairing_flags_ = key_based_pairing_flags;
  }

  const std::string& metadata_id() const { return metadata_id_; }

  const std::string& ble_address() const { return ble_address_; }

  Protocol protocol() const { return protocol_; }

 private:
  friend class base::RefCounted<Device>;
  ~Device();

  // An identifier which components can use to fetch additional metadata for
  // this device. This ID will correspond to different things depending on
  // |protocol_|. For example, if |protocol_| is Fast Pair, this ID will be the
  // model ID of the Fast Pair device.
  const std::string metadata_id_;

  // Bluetooth LE address of the device.
  const std::string ble_address_;

  // The Quick Pair protocol implementation that this device belongs to.
  const Protocol protocol_;

  // Bluetooth classic address of the device.
  std::optional<std::string> classic_address_;

  // Display name for the device
  // Similar to Bluetooth classic address field, this will be null when a
  // device is found from a discoverable advertisement due to the fact that
  // initial pair notifications show the OEM default name from the device
  // metadata instead of the display name.
  std::optional<std::string> display_name_;

  // Fast Pair version number, possible versions numbers are defined at the top
  // of this file.
  std::optional<DeviceFastPairVersion> version_;

  // Account key which will be saved to the user's account during Fast Pairing
  // for eligible devices (V2 or higher) and used for detecting subsequent
  // pairing scenarios.
  std::optional<std::vector<uint8_t>> account_key_;

  // Flags received during a Key-based Pairing Extended Response.
  std::optional<uint8_t> key_based_pairing_flags_;
};

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
std::ostream& operator<<(std::ostream& stream, const Device& device);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
std::ostream& operator<<(std::ostream& stream, scoped_refptr<Device> device);

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_DEVICE_H_
