// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_IMAGE_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_IMAGE_VIEW_H_

#include <memory>
#include <string>

#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/search_result_base_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class ImageButton;
}  // namespace views

namespace ash {
class SearchResultImageListView;

// Displays a search result in the form of an unlabeled image.
class ASH_EXPORT SearchResultImageView : public SearchResultBaseView {
 public:
  METADATA_HEADER(SearchResultImageView);
  explicit SearchResultImageView(SearchResultImageListView* list_view,
                                 std::string dummy_result_id);
  SearchResultImageView(const SearchResultImageView&) = delete;
  SearchResultImageView& operator=(const SearchResultImageView&) = delete;
  ~SearchResultImageView() override;

  void OnImageViewPressed(const ui::Event& event);

  // Overridden from views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  // SearchResultBaseView overrides:
  void OnResultChanged() override;

  SearchResultImageListView* list_view() { return list_view_; }

 private:
  // SearchResultObserver overrides:
  void OnMetadataChanged() override;

  // Owned by views hierarchy.
  raw_ptr<views::ImageButton> result_image_ = nullptr;

  // Parent list view. Owned by views hierarchy.
  raw_ptr<SearchResultImageListView, ExperimentalAsh> const list_view_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_INLINE_ICON_VIEW_H_
