// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_MENU_GROUP_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_MENU_GROUP_H_

#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace ash {

// Defines a view that groups together related capture mode settings in an
// independent section in the settings menu. Each group has a header icon and a
// header label.
class ASH_EXPORT CaptureModeMenuGroup : public views::View {
 public:
  METADATA_HEADER(CaptureModeMenuGroup);

  CaptureModeMenuGroup(const gfx::VectorIcon& header_icon,
                       int header_label_string_id);
  CaptureModeMenuGroup(const CaptureModeMenuGroup&) = delete;
  CaptureModeMenuGroup& operator=(const CaptureModeMenuGroup&) = delete;
  ~CaptureModeMenuGroup() override;

  // Adds an option which has text and a checked image icon to the the menu
  // group. When the option is selected, its checked icon is visible. Otherwise
  // its checked icon is invisible. One and only one option's checked icon is
  // visible all the time.
  void AddOption(views::Button::PressedCallback callback,
                 int string_id,
                 bool checked);

  // Adds a menu item which has text to the menu group. Each menu item can have
  // its own customized behavior. For example, file save menu group's menu item
  // will open a folder window for user to select a new folder to save the
  // captured filed on click/press.
  void AddMenuItem(views::Button::PressedCallback callback, int string_id);
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_MENU_GROUP_H_