// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_THUMBNAIL_H_
#define ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_THUMBNAIL_H_

#include "ash/ash_export.h"
#include "chromeos/components/phonehub/camera_roll_item.h"
#include "ui/views/controls/button/button.h"

namespace ash {

class ASH_EXPORT CameraRollThumbnail : public views::Button {
 public:
  CameraRollThumbnail(const chromeos::phonehub::CameraRollItem& item);
  ~CameraRollThumbnail() override;
  CameraRollThumbnail(CameraRollThumbnail&) = delete;
  CameraRollThumbnail operator=(CameraRollThumbnail&) = delete;

  const char* GetClassName() const override;

 private:
  void ButtonPressed();

  const std::string key_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_THUMBNAIL_H_
