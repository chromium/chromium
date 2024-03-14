// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_PINE_ITEM_VIEW_H_
#define ASH_WM_WINDOW_RESTORE_PINE_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "base/task/cancelable_task_tracker.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace views {
class ImageView;
}

namespace ash {

// Represents an app that will be shown in the pine widget. Contains the app
// title and app icon. Optionally contains a couple favicons depending on the
// app.
// TODO(sammiequon): Add ASCII art.
class ASH_EXPORT PineItemView : public views::BoxLayoutView {
  METADATA_HEADER(PineItemView, views::BoxLayoutView)

 public:
  PineItemView(const std::u16string& app_title,
               const std::vector<GURL>& favicons,
               const size_t tab_count);
  PineItemView(const PineItemView&) = delete;
  PineItemView& operator=(const PineItemView&) = delete;
  ~PineItemView() override;

  views::ImageView* image_view() { return image_view_; }

  base::WeakPtr<PineItemView> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  friend class PineItemViewTestApi;

  void OnOneFaviconLoaded(
      base::OnceCallback<void(const gfx::ImageSkia&)> callback,
      const gfx::ImageSkia& favicon);

  void OnAllFaviconsLoaded(const std::vector<gfx::ImageSkia>& favicons);

  const size_t tab_count_;

  // Owned by views hierarchy.
  raw_ptr<views::ImageView> image_view_;
  raw_ptr<views::BoxLayoutView> favicon_container_view_;

  base::CancelableTaskTracker cancelable_favicon_task_tracker_;

  base::WeakPtrFactory<PineItemView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_PINE_ITEM_VIEW_H_
