// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_IMAGE_LIST_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_IMAGE_LIST_VIEW_H_

#include <vector>

#include "ash/app_list/views/search_result_container_view.h"
#include "ash/app_list/views/search_result_image_view.h"
#include "ash/app_list/views/search_result_image_view_delegate.h"
#include "base/files/file.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class FlexLayoutView;
class Label;
}  // namespace views

namespace ash {

// SearchResultImageListView displays a horizontal strip of
// SearchResultImageViews inside the AppListSearchView.
class ASH_EXPORT SearchResultImageListView : public SearchResultContainerView {
  METADATA_HEADER(SearchResultImageListView, SearchResultContainerView)

 public:
  explicit SearchResultImageListView(AppListViewDelegate* view_delegate);
  SearchResultImageListView(const SearchResultImageListView&) = delete;
  SearchResultImageListView& operator=(const SearchResultImageListView&) =
      delete;
  ~SearchResultImageListView() override;

  // Called when the search result is activated.
  void SearchResultActivated(SearchResultImageView* view,
                             int event_flags,
                             bool by_button_press);

  // Overridden from SearchResultContainerView:
  SearchResultImageView* GetResultViewAt(size_t index) override;

  // Returns all search result image views children of this view.
  std::vector<raw_ptr<SearchResultImageView, VectorExperimental>>
  GetSearchResultImageViews();

  // Returns the preferred width of the image search result according to the
  // layout.
  void ConfigureLayoutForAvailableWidth(int width);

  // A callback that is called when the `info` is loaded. This updates the
  // `image_info_container_` if needed.
  void OnImageMetadataLoaded(base::File::Info info);

  const views::FlexLayoutView* image_info_container_for_test() const {
    return image_info_container_.get();
  }
  const std::vector<raw_ptr<views::Label, VectorExperimental>>&
  metadata_content_labels_for_test() const {
    return metadata_content_labels_;
  }

 private:
  // Overridden from SearchResultContainerView:
  int DoUpdate() override;
  void UpdateResultsVisibility(bool force_hide) override;
  views::View* GetTitleLabel() override;
  std::vector<views::View*> GetViewsToAnimate() override;

  // Delegate for search result image views that implements support for context
  // menu and drag-and-drop operations.
  SearchResultImageViewDelegate delegate_;

  // Owned by views hierarchy.
  raw_ptr<views::Label> title_label_ = nullptr;
  raw_ptr<views::FlexLayoutView> image_view_container_ = nullptr;
  raw_ptr<views::FlexLayoutView> image_info_container_ = nullptr;

  std::vector<raw_ptr<SearchResultImageView, VectorExperimental>> image_views_;

  // Labels that show the file metadata in `image_info_container_`. There should
  // always be 3 labels, which in the order of {file name, file directory, date
  // modified}.
  std::vector<raw_ptr<views::Label, VectorExperimental>>
      metadata_content_labels_;

  base::WeakPtrFactory<SearchResultImageListView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_IMAGE_LIST_VIEW_H_
