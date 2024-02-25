// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_UKM_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_UKM_H_

#include <string>

#include "chrome/browser/ash/arc/input_overlay/constants.h"

namespace arc::input_overlay {

// A class is needed to access the private static functions of
// AppSourceUrlRecorder.
class InputOverlayUkm {
 public:
  static void RecordInputOverlayFeatureStateUkm(std::string package_name,
                                                bool enable);

  static void RecordInputOverlayMappingHintStateUkm(std::string package_name,
                                                    bool enable);

  static void RecordInputOverlayCustomizedUsageUkm(std::string package_name);

  static void RecordInputOverlayActionReposition(
      const std::string& package_name,
      RepositionType reposition_type,
      InputOverlayWindowStateType state_type);

  static void RecordInputOverlayMenuEntryReposition(
      const std::string& package_name,
      RepositionType reposition_type,
      InputOverlayWindowStateType state_type);

  static void RecordInputOverlayButtonGroupReposition(
      const std::string& package_name,
      RepositionType reposition_type,
      InputOverlayWindowStateType state_type);
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_ARC_INPUT_OVERLAY_UKM_H_
