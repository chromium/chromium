// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/overflow_button.h"

#include <memory>

#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/shelf/shelf_view.h"
#include "ash/strings/grit/ash_strings.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/fill_layout.h"

namespace ash {

OverflowButton::OverflowButton(ShelfView* shelf_view)
    : ShelfControlButton(shelf_view->shelf(), shelf_view),
      shelf_view_(shelf_view) {
  SetAccessibleName(l10n_util::GetStringUTF16(IDS_ASH_SHELF_OVERFLOW_NAME));

  horizontal_dots_image_view_ = new views::ImageView();
  horizontal_dots_image_view_->SetImage(
      gfx::CreateVectorIcon(kShelfOverflowHorizontalDotsIcon,
                            ShelfConfig::Get()->shelf_icon_color()));
  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(horizontal_dots_image_view_);
}

OverflowButton::~OverflowButton() = default;

bool OverflowButton::ShouldEnterPushedState(const ui::Event& event) {
  if (shelf_view_->IsShowingOverflowBubble())
    return false;

  // We bypass out direct superclass on purpose here.
  return Button::ShouldEnterPushedState(event);
}

void OverflowButton::NotifyClick(const ui::Event& event) {
  // For this button, do not call the superclass's handler to avoid the ink
  // drop.
  shelf_button_delegate()->ButtonPressed(this, event, nullptr);
}

const char* OverflowButton::GetClassName() const {
  return "ash/OverflowButton";
}

}  // namespace ash
