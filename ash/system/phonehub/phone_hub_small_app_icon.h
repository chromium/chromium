// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_SMALL_APP_ICON_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_SMALL_APP_ICON_H_
#include "ash/ash_export.h"
#include "ui/views/controls/image_view.h"

namespace ash {

class ASH_EXPORT SmallAppIcon : public views::ImageView {
 public:
  SmallAppIcon(const gfx::Image& icon);
  SmallAppIcon(const SmallAppIcon&) = delete;
  SmallAppIcon& operator=(const SmallAppIcon&) = delete;

  ~SmallAppIcon() override = default;

  // views::View:
  const char* GetClassName() const override;
};
}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_SMALL_APP_ICON_H_