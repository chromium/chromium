// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_IMAGE_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_IMAGE_VIEW_H_

#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class ImageButton;
}  // namespace views

namespace ash {
class PulsingBlockView;
class SearchResultImageListView;
class SearchResultImageViewDelegate;

// Displays a search result in the form of an unlabeled image.
class ASH_EXPORT SearchResultImageView : public SearchResultBaseView {
  METADATA_HEADER(SearchResultImageView, SearchResultBaseView)

 public:
  SearchResultImageView(int index,
                        SearchResultImageListView* list_view,
                        SearchResultImageViewDelegate* image_view_delegate);
  SearchResultImageView(const SearchResultImageView&) = delete;
  SearchResultImageView& operator=(const SearchResultImageView&) = delete;
  ~SearchResultImageView() override;

  void OnImageViewPressed(const ui::Event& event);

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // Updates `preferred_width_`.
  void ConfigureLayoutForAvailableWidth(int width);

  // SearchResultBaseView overrides:
  void OnResultChanged() override;

  // Creates a child pulsing block view as a placeholder.
  void CreatePulsingBlockView();

  // Returns true if the image view has a valid result and icon.
  bool HasValidResultIcon();

  // Creates the image skia that is used for dragged image view.
  gfx::ImageSkia CreateDragImage();

  SearchResultImageListView* list_view() { return list_view_; }

  PulsingBlockView* pulsing_block_view_for_test() const {
    return pulsing_block_view_;
  }
  views::ImageButton* result_image_for_test() const { return result_image_; }

 private:
  // SearchResultObserver overrides:
  void OnMetadataChanged() override;

  // The index of this view in its parent `list_view_`.
  const int index_;

  // Owned by views hierarchy.
  raw_ptr<views::ImageButton> result_image_ = nullptr;

  // Parent list view. Owned by views hierarchy.
  raw_ptr<SearchResultImageListView> const list_view_;

  // Child pulsing block view that is used as a placeholder.
  raw_ptr<PulsingBlockView, DanglingUntriaged> pulsing_block_view_ = nullptr;

  // The preferred width of the image view which is used to calculate the
  // preferred size. This is set by the parent container view so that the image
  // views share the space equally.
  int preferred_width_ = 0;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_IMAGE_VIEW_H_
