// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_context_menu_model.h"

namespace ash {

PineContextMenuModel::PineContextMenuModel() : ui::SimpleMenuModel(this) {
  const int group = 0;
  AddRadioItem(static_cast<int>(CommandId::kAskEveryTime), u"Ask every time",
               group);
  AddRadioItem(static_cast<int>(CommandId::kAlways), u"Always restore", group);
  AddRadioItem(static_cast<int>(CommandId::kOff), u"Off", group);
}

PineContextMenuModel::~PineContextMenuModel() = default;

bool PineContextMenuModel::IsCommandIdChecked(int command_id) const {
  return static_cast<CommandId>(command_id) == current_radio_;
}

void PineContextMenuModel::ExecuteCommand(int command_id, int event_flags) {
  switch (static_cast<CommandId>(command_id)) {
    case CommandId::kAskEveryTime:
    case CommandId::kAlways:
    case CommandId::kOff:
      current_radio_ = static_cast<CommandId>(command_id);
      break;
  }
}

}  // namespace ash
