// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_item_view.h"

#include <utility>

#include "ash/style/style_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/controls/button/button.h"

namespace ash {

PickerItemView::PickerItemView(SelectItemCallback select_item_callback)
    : views::Button(std::move(select_item_callback)) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetMasksToBounds(true);
  StyleUtil::SetUpInkDropForButton(this, gfx::Insets(),
                                   /*highlight_on_hover=*/true,
                                   /*highlight_on_focus=*/true);
}

PickerItemView::~PickerItemView() = default;

BEGIN_METADATA(PickerItemView)
END_METADATA

}  // namespace ash
