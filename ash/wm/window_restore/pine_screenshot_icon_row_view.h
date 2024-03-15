// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_PINE_SCREENSHOT_ICON_ROW_VIEW_H_
#define ASH_WM_WINDOW_RESTORE_PINE_SCREENSHOT_ICON_ROW_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/window_restore/pine_contents_data.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace views {
class ImageView;
}  // namespace views

namespace ash {

// TODO(hewer|sammiequon|minch): There are some duplicates among this class,
// PineItemsOverflowView and PineItemsContainerView. Especially the logic to get
// the app icons, tab fav icons. See whether we can do some refactoring to
// reduce the duplicate after finishing all the functionality.
//
// The view holds a row of icons resides at the bottom-left of the pine
// screenshot preview.
class ASH_EXPORT PineScreenshotIconRowView : public views::BoxLayoutView {
  METADATA_HEADER(PineScreenshotIconRowView, views::BoxLayoutView)

 public:
  explicit PineScreenshotIconRowView(
      const PineContentsData::AppsInfos& apps_infos);
  PineScreenshotIconRowView(const PineScreenshotIconRowView&) = delete;
  PineScreenshotIconRowView& operator=(const PineScreenshotIconRowView&) =
      delete;
  ~PineScreenshotIconRowView() override;

 private:
  friend class PineScreenshotIconRowViewTestApi;

  // The callback to set the `icon` to the image view at the `index` of
  // `image_view_map_`.
  void SetIconForIndex(int index, const gfx::ImageSkia& icon);

  // views::View:
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // The map stores the ImageView's position inside the icon row. The image view
  // will be created in order, but its icon will be set later in unpredicted
  // order as the call to get the icon can be asynchronously.
  base::flat_map<int, views::ImageView*> image_view_map_;

  base::WeakPtrFactory<PineScreenshotIconRowView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_PINE_SCREENSHOT_ICON_ROW_VIEW_H_
