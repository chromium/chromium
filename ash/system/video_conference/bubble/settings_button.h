// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_SETTINGS_BUTTON_H_
#define ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_SETTINGS_BUTTON_H_

#include "ash/style/icon_button.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace ash::video_conference {

// The settings button, consisting of a gear icon and a drop down arrow icon,
// opens the VC settings menu when clicked.
class SettingsButton : public views::Button {
  METADATA_HEADER(SettingsButton, views::Button)

 public:
  SettingsButton();
  SettingsButton(const SettingsButton&) = delete;
  SettingsButton& operator=(const SettingsButton&) = delete;
};

}  // namespace ash::video_conference

#endif  // ASH_SYSTEM_VIDEO_CONFERENCE_BUBBLE_SETTINGS_BUTTON_H_
