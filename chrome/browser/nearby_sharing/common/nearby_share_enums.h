// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_ENUMS_H_
#define CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_ENUMS_H_

#include "chromeos/ash/services/nearby/public/mojom/nearby_share_settings.mojom.h"

// Represents the advertising bluetooth power for Nearby Connections.
enum class PowerLevel {
  kUnknown = 0,
  kLowPower = 1,
  kMediumPower = 2,
  kHighPower = 3,
  kMaxValue = kHighPower
};

// TODO(https://crbug.com/1106369): these names are too generic for the global
// namespace
using DataUsage = nearby_share::mojom::DataUsage;
using Visibility = nearby_share::mojom::Visibility;
using FastInitiationNotificationState =
    nearby_share::mojom::FastInitiationNotificationState;

#endif  // CHROME_BROWSER_NEARBY_SHARING_COMMON_NEARBY_SHARE_ENUMS_H_
