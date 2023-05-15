// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_DRAG_DROP_DELEGATE_H_
#define ASH_WALLPAPER_WALLPAPER_DRAG_DROP_DELEGATE_H_

#include <set>

#include "ash/ash_export.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"

namespace gfx {
class Point;
}  // namespace gfx

namespace ui {
class ClipboardFormatType;
class OSExchangeData;
}  // namespace ui

namespace ash {

// The singleton delegate, owned by the `WallpaperControllerImpl`, for
// drag-and-drop events over the wallpaper. Note that the delegate may not exist
// if drag-and-drop related features are disabled.
class ASH_EXPORT WallpaperDragDropDelegate {
 public:
  WallpaperDragDropDelegate();
  WallpaperDragDropDelegate(const WallpaperDragDropDelegate&) = delete;
  WallpaperDragDropDelegate& operator=(const WallpaperDragDropDelegate&) =
      delete;
  virtual ~WallpaperDragDropDelegate();

  // Returns the `formats` and `types` of interesting data. The delegate will
  // not receive events for drag-and-drop sequences which do not contain
  // interesting data.
  virtual void GetDropFormats(int* formats,
                              std::set<ui::ClipboardFormatType>* types);

  // Returns whether the delegate can handle a drop of the specified
  // interesting `data`. If `false` is returned, the delegate will not receive
  // further events for the current drag-and-drop sequence.
  virtual bool CanDrop(const ui::OSExchangeData& data);

  // Invoked when interesting `data` is dragged over the wallpaper at the
  // specified `location_in_screen` coordinates.
  virtual void OnDragEntered(const ui::OSExchangeData& data,
                             const gfx::Point& location_in_screen);

  // Invoked when interesting `data` is dragged within the wallpaper at the
  // specified `location_in_screen` coordinates. This method should return the
  // drag operation supported.
  virtual ui::DragDropTypes::DragOperation OnDragUpdated(
      const ui::OSExchangeData& data,
      const gfx::Point& location_in_screen);

  // Invoked when interesting `data` is dragged out of the wallpaper.
  virtual void OnDragExited();

  // Invoked when interesting `data` is dropped onto the wallpaper at the
  // specified `location_in_screen` coordinates. This method should return the
  // drag operation supported.
  virtual ui::mojom::DragOperation OnDrop(const ui::OSExchangeData& data,
                                          const gfx::Point& location_in_screen);
};

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_DRAG_DROP_DELEGATE_H_
