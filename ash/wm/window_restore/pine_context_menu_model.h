// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_PINE_CONTEXT_MENU_MODEL_H_
#define ASH_WM_WINDOW_RESTORE_PINE_CONTEXT_MENU_MODEL_H_

#include "ash/ash_export.h"
#include "ui/base/models/simple_menu_model.h"

namespace ash {

// A menu model that builds the contents of the Pine settings context menu.
// Created when clicking on the Pine settings button.
class ASH_EXPORT PineContextMenuModel : public ui::SimpleMenuModel,
                                        public ui::SimpleMenuModel::Delegate {
 public:
  PineContextMenuModel();
  PineContextMenuModel(const PineContextMenuModel&) = delete;
  PineContextMenuModel& operator=(const PineContextMenuModel&) = delete;

  ~PineContextMenuModel() override;

  // ui::SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  // TODO(hewer): Link to histograms.
  enum class CommandId { kAskEveryTime, kAlways, kOff };

  // TODO(hewer): Remove temporary radio selection.
  CommandId current_radio_ = CommandId::kAskEveryTime;
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_PINE_CONTEXT_MENU_MODEL_H_
