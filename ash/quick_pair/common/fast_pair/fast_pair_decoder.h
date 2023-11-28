// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_DECODER_H_
#define ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_DECODER_H_

#include <cstdint>
#include <optional>
#include <vector>

#include "base/component_export.h"

namespace ash {
namespace quick_pair {
namespace fast_pair_decoder {

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
int GetVersion(const std::vector<uint8_t>* service_data);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
int GetIdLength(const std::vector<uint8_t>* service_data);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
bool HasModelId(const std::vector<uint8_t>* service_data);

COMPONENT_EXPORT(QUICK_PAIR_COMMON)
std::optional<std::string> GetHexModelIdFromServiceData(
    const std::vector<uint8_t>* service_data);

}  // namespace fast_pair_decoder
}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_FAST_PAIR_FAST_PAIR_DECODER_H_
