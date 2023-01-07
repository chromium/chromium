// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_MENU_MODEL_H_
#define ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_MENU_MODEL_H_

#include "ash/ash_export.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {

class ASH_EXPORT CameraRollMenuModel : public ui::SimpleMenuModel,
                                       public ui::SimpleMenuModel::Delegate {
 public:
  explicit CameraRollMenuModel(const base::RepeatingClosure download_callback);
  ~CameraRollMenuModel() override;
  CameraRollMenuModel(const CameraRollMenuModel&) = delete;
  CameraRollMenuModel& operator=(const CameraRollMenuModel&) = delete;

  enum CommandID {
    COMMAND_DOWNLOAD,
  };

  // ui::SimpleMenuModel::Delegate:
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  const base::RepeatingClosure download_callback_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_CAMERA_ROLL_MENU_MODEL_H_
