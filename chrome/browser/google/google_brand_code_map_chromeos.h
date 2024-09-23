// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GOOGLE_GOOGLE_BRAND_CODE_MAP_CHROMEOS_H_
#define CHROME_BROWSER_GOOGLE_GOOGLE_BRAND_CODE_MAP_CHROMEOS_H_

#include <optional>
#include <string_view>

#include "components/policy/core/common/cloud/cloud_policy_constants.h"

namespace google_brand {
namespace chromeos {

// Returns |static_brand_code| if it is not found in the map. Otherwise, returns
// a variation of the brand code based on |market_segment| (an empty value
// indicates the device is not enrolled).
std::string_view GetRlzBrandCode(
    std::string_view static_brand_code,
    std::optional<policy::MarketSegment> market_segment);

}  // namespace chromeos
}  // namespace google_brand

#endif  // CHROME_BROWSER_GOOGLE_GOOGLE_BRAND_CODE_MAP_CHROMEOS_H_
