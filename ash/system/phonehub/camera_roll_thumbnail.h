// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_THUMBNAIL_H_
#define ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_THUMBNAIL_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/camera_roll_menu_model.h"
#include "ash/system/phonehub/phone_hub_metrics.h"
#include "chromeos/components/phonehub/camera_roll_item.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace chromeos {
namespace phonehub {
class CameraRollManager;
class UserActionRecorder;
}  // namespace phonehub
}  // namespace chromeos

namespace ash {

class ASH_EXPORT CameraRollThumbnail : public views::MenuButton {
 public:
  CameraRollThumbnail(
      const int index,
      const chromeos::phonehub::CameraRollItem& item,
      chromeos::phonehub::CameraRollManager* camera_roll_manager,
      chromeos::phonehub::UserActionRecorder* user_action_recorder);
  ~CameraRollThumbnail() override;
  CameraRollThumbnail(CameraRollThumbnail&) = delete;
  CameraRollThumbnail operator=(CameraRollThumbnail&) = delete;

  // views::MenuButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;

 private:
  void ButtonPressed();
  ui::SimpleMenuModel* GetMenuModel();
  void DownloadRequested();
  phone_hub_metrics::CameraRollMediaType GetMediaType();

  const int index_;
  const chromeos::phonehub::proto::CameraRollItemMetadata metadata_;
  const bool video_type_;
  const gfx::ImageSkia image_;

  std::unique_ptr<CameraRollMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  chromeos::phonehub::CameraRollManager* camera_roll_manager_ = nullptr;
  chromeos::phonehub::UserActionRecorder* user_action_recorder_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_THUMBNAIL_H_
