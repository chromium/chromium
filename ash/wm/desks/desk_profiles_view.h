// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_PROFILES_VIEW_H_
#define ASH_WM_DESKS_DESK_PROFILES_VIEW_H_

#include "ash/wm/desks/desk.h"
#include "base/memory/raw_ptr.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"

namespace ash {

class DeskProfilesButton : public views::ImageButton, public Desk::Observer {
 public:
  explicit DeskProfilesButton(views::Button::PressedCallback callback,
                              Desk* desk);
  DeskProfilesButton(const DeskProfilesButton&) = delete;
  DeskProfilesButton& operator=(const DeskProfilesButton&) = delete;
  ~DeskProfilesButton() override;

  void UpdateIcon();

  // Desk::Observer:
  void OnContentChanged() override {}
  void OnDeskDestroyed(const Desk* desk) override;
  void OnDeskNameChanged(const std::u16string& new_name) override {}

 private:
  // The associated desk.
  raw_ptr<Desk, ExperimentalAsh> desk_;  // Not owned.
  raw_ptr<views::ImageView, ExperimentalAsh> icon_ = nullptr;
  gfx::ImageSkia icon_image_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_PROFILES_VIEW_H_
