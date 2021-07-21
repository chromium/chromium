// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_DEVICE_H_
#define ASH_QUICK_PAIR_COMMON_DEVICE_H_

#include "ash/quick_pair/common/protocol.h"
#include "base/component_export.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"

namespace ash {
namespace quick_pair {

// Thin class which is used by the higher level components of the Quick Pair
// system to represent a device.
//
// Lower level components will use |protocol|, |metadata_id| and |address| to
// fetch objects which contain more information. E.g. A Fast Pair component
// can use |metadata_id| to query the Service to receive a full metadata
// object.
struct COMPONENT_EXPORT(QUICK_PAIR_COMMON) Device
    : public base::RefCounted<Device> {
  Device(std::string metadata_id, std::string address, Protocol protocol);
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;
  Device& operator=(Device&&) = delete;

  // An identifier which components can use to fetch additional metadata for
  // this device. This ID will correspond to different things depending on
  // |protocol|. For example, if |protocol| is Fast Pair, this ID will be the
  // model ID of the Fast Pair device.
  const std::string metadata_id;

  // Bluetooth address of the device.
  const std::string address;

  // The Quick Pair protocol implementation that this device belongs to.
  const Protocol protocol;

 private:
  friend class base::RefCounted<Device>;
  ~Device() = default;
};

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
std::ostream& operator<<(std::ostream& stream, const Device& device);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
std::ostream& operator<<(std::ostream& stream, scoped_refptr<Device> device);

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_DEVICE_H_
