// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_ECHE_APP_UI_FEATURE_STATUS_H_
#define ASH_WEBUI_ECHE_APP_UI_FEATURE_STATUS_H_

#include <ostream>

namespace ash {
namespace eche_app {

// Enum representing potential status values for the eche feature.
enum class FeatureStatus {
  // The user's devices are not eligible for the feature. This means that either
  // the Chrome OS device or the user's phone (or both) have not enrolled with
  // the requisite feature enum values, OR parent features are not enabled.
  kIneligible = 0,

  // The feature is disabled, but the user could enable it via settings.
  kDisabled = 1,

  // The feature is enabled, but currently there is no active connection to
  // the phone.
  kDisconnected = 2,

  // The feature is enabled, and there is an active attempt to connect to the
  // phone.
  kConnecting = 3,

  // The feature is enabled, and there is an active connection with the phone.
  kConnected = 4,

  // A dependent feature is in an incompatible state.
  kDependentFeature = 5,

  // A dependent feature is in an incompatible state because there is no active
  // connection to the phone or trying to connect to the phone.
  kDependentFeaturePending = 6,
};

std::ostream& operator<<(std::ostream& stream, FeatureStatus status);

}  // namespace eche_app
}  // namespace ash

#endif  // ASH_WEBUI_ECHE_APP_UI_FEATURE_STATUS_H_
