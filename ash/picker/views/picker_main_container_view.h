// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_MAIN_CONTAINER_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_MAIN_CONTAINER_VIEW_H_

#include <memory>
#include <utility>

#include "ash/ash_export.h"
#include "ash/picker/views/picker_contents_view.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

enum class PickerLayoutType;
class PickerSearchFieldView;
class PickerPageView;
class SystemShadow;

// View for the main Picker container, which consists of the search field and
// the main contents (e.g. search results page).
class ASH_EXPORT PickerMainContainerView
    : public views::View,
      public PickerTraversableItemContainer {
  METADATA_HEADER(PickerMainContainerView, views::View)

 public:
  PickerMainContainerView();
  PickerMainContainerView(const PickerMainContainerView&) = delete;
  PickerMainContainerView& operator=(const PickerMainContainerView&) = delete;
  ~PickerMainContainerView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // PickerTraversableItemContainer:
  views::View* GetTopItem() override;
  views::View* GetBottomItem() override;
  views::View* GetItemAbove(views::View* item) override;
  views::View* GetItemBelow(views::View* item) override;
  views::View* GetItemLeftOf(views::View* item) override;
  views::View* GetItemRightOf(views::View* item) override;
  bool ContainsItem(views::View* item) override;

  PickerSearchFieldView* AddSearchFieldView(
      std::unique_ptr<PickerSearchFieldView> search_field_view);

  // Creates and adds the contents view, which will contain the main contents of
  // the container (e.g. search results page).
  PickerContentsView* AddContentsView(PickerLayoutType layout_type);

  template <typename T>
  T* AddPage(std::unique_ptr<T> view) {
    return contents_view_->AddPage(std::move(view));
  }

  PickerPageView* active_page() { return active_page_; }
  void SetActivePage(PickerPageView* page_view);

 private:
  std::unique_ptr<SystemShadow> shadow_;

  raw_ptr<PickerSearchFieldView> search_field_view_ = nullptr;
  raw_ptr<PickerContentsView> contents_view_ = nullptr;

  // The currently visible page of `contents_view_`, or nullptr if there is no
  // such page.
  raw_ptr<PickerPageView> active_page_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_MAIN_CONTAINER_VIEW_H_
