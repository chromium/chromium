// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PHONEHUB_PHONE_HUB_RECENT_APP_BUTTON_H_
#define ASH_SYSTEM_PHONEHUB_PHONE_HUB_RECENT_APP_BUTTON_H_

#include "ash/ash_export.h"
#include "chromeos/ash/components/phonehub/recent_apps_interaction_handler.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

// A recent app button containing a application |icon|. The |callback| provided
// to build PhoneHubRecentAppButton implicitly contains the package name of the
// same application.
class ASH_EXPORT PhoneHubRecentAppButton : public views::ImageButton {
  METADATA_HEADER(PhoneHubRecentAppButton, views::ImageButton)

 public:
  PhoneHubRecentAppButton(const gfx::Image& icon,
                          const std::u16string& visible_app_name,
                          PressedCallback callback);
  ~PhoneHubRecentAppButton() override;
  PhoneHubRecentAppButton(PhoneHubRecentAppButton&) = delete;
  PhoneHubRecentAppButton operator=(PhoneHubRecentAppButton&) = delete;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // views::ImageButton:
  void PaintButtonContents(gfx::Canvas* canvas) override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_PHONEHUB_PHONE_HUB_RECENT_APP_BUTTON_H_
