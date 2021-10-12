// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_DELETE_BUTTON_H_
#define ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_DELETE_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"

namespace ash {

// A button view that shows up when hovering over the associated grid item.
// Allows the user to delete the template.
class DesksTemplatesDeleteButton : public views::ImageButton {
 public:
  METADATA_HEADER(DesksTemplatesDeleteButton);

  DesksTemplatesDeleteButton();
  DesksTemplatesDeleteButton(const DesksTemplatesDeleteButton&) = delete;
  DesksTemplatesDeleteButton& operator=(const DesksTemplatesDeleteButton&) =
      delete;
  ~DesksTemplatesDeleteButton() override;

  // views::ImageButton:
  void OnThemeChanged() override;
};

BEGIN_VIEW_BUILDER(/* no export */,
                   DesksTemplatesDeleteButton,
                   views::ImageButton)
END_VIEW_BUILDER

}  // namespace ash

DEFINE_VIEW_BUILDER(/* no export */, ash::DesksTemplatesDeleteButton)

#endif  // ASH_WM_DESKS_TEMPLATES_DESKS_TEMPLATES_DELETE_BUTTON_H_
