// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/camera_roll_menu_model.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Appearance in dip.
constexpr int kCameraRollMenuIconSize = 20;

}  // namespace

CameraRollMenuModel::CameraRollMenuModel(const std::string key)
    : ui::SimpleMenuModel(this), key_(key) {
  AddItemWithIcon(COMMAND_DOWNLOAD,
                  l10n_util::GetStringUTF16(
                      IDS_ASH_PHONE_HUB_CAMERA_ROLL_MENU_DOWNLOAD_LABEL),
                  ui::ImageModel::FromVectorIcon(
                      kPhoneHubCameraRollMenuDownloadIcon, ui::kColorMenuIcon,
                      kCameraRollMenuIconSize));
}

void CameraRollMenuModel::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case COMMAND_DOWNLOAD: {
      PA_LOG(INFO) << "User requests download of Camera Roll Item key=" << key_;
      break;
    }
  }
}

}  // namespace ash
