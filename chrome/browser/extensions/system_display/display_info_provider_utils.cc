// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/system_display/display_info_provider_utils.h"

#include "extensions/common/api/system_display.h"

namespace extensions {

namespace {

namespace system_display = api::system_display;

system_display::LayoutPosition GetLayoutPositionFromMojo(
    crosapi::mojom::DisplayLayoutPosition position) {
  switch (position) {
    case crosapi::mojom::DisplayLayoutPosition::kTop:
      return system_display::LayoutPosition::LAYOUT_POSITION_TOP;
    case crosapi::mojom::DisplayLayoutPosition::kRight:
      return system_display::LayoutPosition::LAYOUT_POSITION_RIGHT;
    case crosapi::mojom::DisplayLayoutPosition::kBottom:
      return system_display::LayoutPosition::LAYOUT_POSITION_BOTTOM;
    case crosapi::mojom::DisplayLayoutPosition::kLeft:
      return system_display::LayoutPosition::LAYOUT_POSITION_LEFT;
  }
  NOTREACHED();
  return system_display::LayoutPosition::LAYOUT_POSITION_LEFT;
}
}  // namespace

void OnGetDisplayLayoutResult(
    base::OnceCallback<void(DisplayInfoProvider::DisplayLayoutList)> callback,
    crosapi::mojom::DisplayLayoutInfoPtr info) {
  DisplayInfoProvider::DisplayLayoutList result;
  if (info->layouts) {
    for (crosapi::mojom::DisplayLayoutPtr& layout : *info->layouts) {
      api::system_display::DisplayLayout display_layout;
      display_layout.id = layout->id;
      display_layout.parent_id = layout->parent_id;
      display_layout.position = GetLayoutPositionFromMojo(layout->position);
      display_layout.offset = layout->offset;
      result.emplace_back(std::move(display_layout));
    }
  }
  std::move(callback).Run(std::move(result));
}

}  // namespace extensions
