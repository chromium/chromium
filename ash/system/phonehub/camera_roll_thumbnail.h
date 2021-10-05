// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_THUMBNAIL_H_
#define ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_THUMBNAIL_H_

#include "ash/ash_export.h"
#include "ash/system/phonehub/camera_roll_menu_model.h"
#include "chromeos/components/phonehub/camera_roll_item.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/canvas.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_runner.h"

namespace ash {

class ASH_EXPORT CameraRollThumbnail : public views::MenuButton {
 public:
  CameraRollThumbnail(const chromeos::phonehub::CameraRollItem& item);
  ~CameraRollThumbnail() override;
  CameraRollThumbnail(CameraRollThumbnail&) = delete;
  CameraRollThumbnail operator=(CameraRollThumbnail&) = delete;

  // views::MenuButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
  const char* GetClassName() const override;

 private:
  void ButtonPressed();
  ui::SimpleMenuModel* GetMenuModel();

  const std::string key_;
  const bool video_type_;
  const gfx::ImageSkia image_;

  std::unique_ptr<CameraRollMenuModel> menu_model_;
  std::unique_ptr<views::MenuRunner> menu_runner_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_THUMBNAIL_H_
