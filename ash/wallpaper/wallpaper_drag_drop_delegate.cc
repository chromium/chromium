// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_drag_drop_delegate.h"

#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"

namespace ash {

WallpaperDragDropDelegate::WallpaperDragDropDelegate() = default;

WallpaperDragDropDelegate::~WallpaperDragDropDelegate() = default;

void WallpaperDragDropDelegate::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* types) {}

bool WallpaperDragDropDelegate::CanDrop(const ui::OSExchangeData& data) {
  return false;
}

void WallpaperDragDropDelegate::OnDragEntered(
    const ui::OSExchangeData& data,
    const gfx::Point& location_in_screen) {}

ui::DragDropTypes::DragOperation WallpaperDragDropDelegate::OnDragUpdated(
    const ui::OSExchangeData& data,
    const gfx::Point& location_in_screen) {
  return ui::DragDropTypes::DragOperation::DRAG_NONE;
}

void WallpaperDragDropDelegate::OnDragExited() {}

ui::mojom::DragOperation WallpaperDragDropDelegate::OnDrop(
    const ui::OSExchangeData& data,
    const gfx::Point& location_in_screen) {
  return ui::mojom::DragOperation::kNone;
}

}  // namespace ash
