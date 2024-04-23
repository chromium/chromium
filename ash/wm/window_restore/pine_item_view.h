// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_PINE_ITEM_VIEW_H_
#define ASH_WM_WINDOW_RESTORE_PINE_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/window_restore/pine_contents_data.h"
#include "base/task/cancelable_task_tracker.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace views {
class Label;
}

namespace ash {

// Represents an app that will be shown in the pine widget. Contains the app
// title and app icon. Optionally contains a couple favicons depending on the
// app. Or represents the only opened browser window and its favicons inside the
// screenshot preview.
//
// TODO(hewer): Add ASCII art.
class ASH_EXPORT PineItemView : public views::BoxLayoutView {
  METADATA_HEADER(PineItemView, views::BoxLayoutView)

 public:
  using IndexedImagePair = std::pair</*index=*/int, gfx::ImageSkia>;

  // Callback that passes a `gfx::ImageSkia` (favicon) paired with its index in
  // the list of favicons, so we can maintain order after loading.
  using IndexedImageCallback =
      base::OnceCallback<void(const IndexedImagePair&)>;

  PineItemView(const PineContentsData::AppInfo& app_info,
               bool inside_screenshot);
  PineItemView(const PineItemView&) = delete;
  PineItemView& operator=(const PineItemView&) = delete;
  ~PineItemView() override;

  const views::Label* title_label_view() const { return title_label_view_; }

  base::WeakPtr<PineItemView> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnOneFaviconLoaded(IndexedImageCallback callback,
                          int index,
                          const gfx::ImageSkia& favicon);

  void OnAllFaviconsLoaded(std::vector<IndexedImagePair> indexed_favicons);

  // Updates the title label on creation, and if/when the app is updated.
  void UpdateTitle();

  const std::string app_id_;
  const size_t tab_count_;

  // True if this represents the browser window and its favicons inside the
  // screenshot preview.
  const bool inside_screenshot_;

  // Owned by views hierarchy.
  raw_ptr<views::Label> title_label_view_;
  raw_ptr<views::BoxLayoutView> favicon_container_view_;

  base::CancelableTaskTracker cancelable_favicon_task_tracker_;

  base::WeakPtrFactory<PineItemView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_PINE_ITEM_VIEW_H_
