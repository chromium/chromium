// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCELERATORS_ACCELERATOR_IDS_H_
#define ASH_ACCELERATORS_ACCELERATOR_IDS_H_

namespace ash {

// Accelerator ids consist of two parts:
// . Upper 16 bits identifies source (namespace part).
// . Lower 16 supplied from client (local part).
constexpr uint16_t kLocalIdMask = 0xFFFF;

inline uint32_t ComputeAcceleratorId(uint16_t accelerator_namespace,
                                     uint16_t local_id) {
  return (accelerator_namespace << 16) | local_id;
}

inline uint16_t GetAcceleratorLocalId(uint32_t accelerator_id) {
  return static_cast<uint16_t>(accelerator_id & kLocalIdMask);
}

inline uint16_t GetAcceleratorNamespaceId(uint32_t accelerator_id) {
  return static_cast<uint16_t>((accelerator_id >> 16) & kLocalIdMask);
}

}  // namespace ash

#endif  // ASH_ACCELERATORS_ACCELERATOR_IDS_H_
