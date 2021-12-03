// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HPS_HPS_CONFIGURATION_H_
#define ASH_SYSTEM_HPS_HPS_CONFIGURATION_H_

#include "ash/ash_export.h"
#include "chromeos/dbus/hps/hps_service.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ash {

// Gets FeatureConfig for enabling HpsSense from Finch.
// Returns nullopt if feature is not enabled or can't be parsed correctly.
ASH_EXPORT absl::optional<hps::FeatureConfig> GetEnableHpsSenseConfig();

// Gets FeatureConfig for enabling HpsNotify from Finch.
// Returns nullopt if feature is not enabled or can't be parsed correctly.
ASH_EXPORT absl::optional<hps::FeatureConfig> GetEnableHpsNotifyConfig();

// Gets quick dim delay to configure power_manager.
ASH_EXPORT base::TimeDelta GetQuickDimDelay();

// If true, quick dim functionality should be temporarily disabled when a quick
// dim is undimmed within a short period of time.
// Used to configure power_manager.
ASH_EXPORT bool GetQuickDimFeedbackEnabled();

}  // namespace ash

#endif  // ASH_SYSTEM_HPS_HPS_CONFIGURATION_H_