// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_ITEM_VIEW_H_
#define ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ash/wm/window_restore/informed_restore_contents_data.h"
#include "base/task/cancelable_task_tracker.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace views {
class Label;
}

namespace ash {

// Represents an app that will be shown in the informed restore widget. Contains
// the app title and app icon. Optionally contains a couple favicons depending
// on the app. Or represents the only opened browser window and its favicons
// inside the screenshot preview.
//
//   +----------------------------------------------+
//   |  +-------+   +-----------------------+       |
//   |  |       |   |                       |       |
//   |  |       |   +-----------------------+       |
//   |  |       |   +-+ +-+ +-+ +-+         ^       |
//   |  +-------+   +-+ +-+ +-+ +-+         |       |
//   +--^---------------^-------------------|-------+
//   ^  |               |                   `Label`
//   |  `ImageView`     |
//   |                  `ImageView` inside `BoxLayoutView` (Chrome App)
//   |
//   `InformedRestoreItemView`
class ASH_EXPORT InformedRestoreItemView : public views::BoxLayoutView {
  METADATA_HEADER(InformedRestoreItemView, views::BoxLayoutView)

 public:
  using IndexedImagePair = std::pair</*index=*/int, gfx::ImageSkia>;

  // Callback that passes a `gfx::ImageSkia` (favicon) paired with its index in
  // the list of favicons, so we can maintain order after loading.
  using IndexedImageCallback =
      base::OnceCallback<void(const IndexedImagePair&)>;

  InformedRestoreItemView(const InformedRestoreContentsData::AppInfo& app_info,
               bool inside_screenshot);
  InformedRestoreItemView(const InformedRestoreItemView&) = delete;
  InformedRestoreItemView& operator=(const InformedRestoreItemView&) = delete;
  ~InformedRestoreItemView() override;

  const views::Label* title_label_view() const { return title_label_view_; }

  base::WeakPtr<InformedRestoreItemView> GetWeakPtr() {
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
  const std::string default_title_;

  const size_t tab_count_;

  // True if this represents the browser window and its favicons inside the
  // screenshot preview.
  const bool inside_screenshot_;

  // Owned by views hierarchy.
  raw_ptr<views::Label> title_label_view_;
  raw_ptr<views::BoxLayoutView> favicon_container_view_;

  base::CancelableTaskTracker cancelable_favicon_task_tracker_;

  base::WeakPtrFactory<InformedRestoreItemView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_WINDOW_RESTORE_INFORMED_RESTORE_ITEM_VIEW_H_
