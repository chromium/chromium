// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOGIN_UI_MANAGEMENT_BUBBLE_H_
#define ASH_LOGIN_UI_MANAGEMENT_BUBBLE_H_

#include <string>

#include "ash/login/ui/lock_contents_view_constants.h"
#include "ash/login/ui/login_tooltip_view.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"

namespace ash {

class ManagementBubble : public LoginTooltipView {
  METADATA_HEADER(ManagementBubble, LoginTooltipView)

 public:
  ManagementBubble(const std::u16string& message,
                   base::WeakPtr<views::View> anchor_view);
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
};

}  // namespace ash
#endif  // ASH_LOGIN_UI_MANAGEMENT_BUBBLE_H_
