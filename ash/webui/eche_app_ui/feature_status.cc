// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/eche_app_ui/feature_status.h"

namespace ash {
namespace eche_app {

std::ostream& operator<<(std::ostream& stream, FeatureStatus status) {
  switch (status) {
    case FeatureStatus::kIneligible:
      stream << "[Ineligible for feature]";
      break;
    case FeatureStatus::kDisabled:
      stream << "[Disabled]";
      break;
    case FeatureStatus::kDisconnected:
      stream << "[Enabled; disconnected]";
      break;
    case FeatureStatus::kConnecting:
      stream << "[Enabled; connecting]";
      break;
    case FeatureStatus::kConnected:
      stream << "[Enabled; connected]";
      break;
    case FeatureStatus::kDependentFeature:
      stream << "[Dependent feature not in a compatible state]";
      break;
    case FeatureStatus::kDependentFeaturePending:
      stream << "[Dependent feature is in a pending state]";
      break;
  }

  return stream;
}

}  // namespace eche_app
}  // namespace ash
