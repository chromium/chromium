// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PRIVACY_PRIVACY_INDICATORS_CONTROLLER_H_
#define ASH_SYSTEM_PRIVACY_PRIVACY_INDICATORS_CONTROLLER_H_

#include <string>

#include "ash/ash_export.h"

namespace ash {

// Add, update, or remove the privacy notification associated with the given
// `app_id`.
void ASH_EXPORT ModifyPrivacyIndicatorsNotification(const std::string& app_id,
                                                    bool camera_is_used,
                                                    bool microphone_is_used);

}  // namespace ash

#endif  // ASH_SYSTEM_PRIVACY_PRIVACY_INDICATORS_CONTROLLER_H_
