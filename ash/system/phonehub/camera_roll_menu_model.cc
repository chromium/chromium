// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/camera_roll_menu_model.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// Appearance in dip.
constexpr int kCameraRollMenuIconSize = 20;

}  // namespace

CameraRollMenuModel::CameraRollMenuModel(
    const base::RepeatingClosure download_callback)
    : ui::SimpleMenuModel(this),
      download_callback_(std::move(download_callback)) {
  AddItemWithIcon(COMMAND_DOWNLOAD,
                  l10n_util::GetStringUTF16(
                      IDS_ASH_PHONE_HUB_CAMERA_ROLL_MENU_DOWNLOAD_LABEL),
                  ui::ImageModel::FromVectorIcon(
                      kPhoneHubCameraRollMenuDownloadIcon,
                      ui::kColorAshSystemUIMenuIcon, kCameraRollMenuIconSize));
}

CameraRollMenuModel::~CameraRollMenuModel() {}

void CameraRollMenuModel::ExecuteCommand(int command_id, int event_flags) {
  switch (command_id) {
    case COMMAND_DOWNLOAD: {
      download_callback_.Run();
      break;
    }
  }
}

}  // namespace ash
