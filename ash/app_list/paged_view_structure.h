// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_PAGED_VIEW_STRUCTURE_H_
#define ASH_APP_LIST_PAGED_VIEW_STRUCTURE_H_

#include <vector>

#include "ash/ash_export.h"
#include "base/check_op.h"
#include "base/macros.h"

namespace ash {

class AppsGridView;
class AppListItemView;
struct GridIndex;

// Manages the mapping between AppListItemList index, view model index, and
// visual pages in the apps grid view.
class ASH_EXPORT PagedViewStructure {
 public:
  using Page = std::vector<AppListItemView*>;
  using Pages = std::vector<Page>;

  explicit PagedViewStructure(AppsGridView* apps_grid_view);
  PagedViewStructure(const PagedViewStructure& other);
  ~PagedViewStructure();

  enum class Mode {
    // Paged, with partially full pages created by page break items.
    kPartialPages,
    // Paged, with all pages full. Used for folders.
    kFullPages,
    // A single long page. Ignores page breaks in the data model. Used for
    // scrollable apps grid.
    kSinglePage
  };
  void Init(Mode mode);

  // Loads the view structure based on the position and page position in the
  // metadata of item views in the view model.
  void LoadFromMetadata();

  // Saves page position change of each item view to metadata of item views
  // in the view model.
  void SaveToMetadata();

  // Operations allowed to modify the view structure. Populates overflowing item
  // views to next page if |clear_overflow| is true. Clears empty pages if
  // |clear_empty_pages| is true. Both are true by default.
  void Move(AppListItemView* view,
            const GridIndex& target_index,
            bool clear_overflow = true,
            bool clear_empty_pages = true);
  void Remove(AppListItemView* view,
              bool clear_overflow = true,
              bool clear_empty_pages = true);
  void Add(AppListItemView* view,
           const GridIndex& target_index,
           bool clear_overflow = true,
           bool clear_empty_pages = true);

  // Convert between the model index and the visual index. The model index
  // is the index of the item in AppListModel (Also the same index in
  // AppListItemList). The visual index is the GridIndex struct above with
  // page/slot info of where to display the item.
  GridIndex GetIndexFromModelIndex(int model_index) const;
  int GetModelIndexFromIndex(const GridIndex& index) const;

  // Returns the last possible visual index to add an item view.
  GridIndex GetLastTargetIndex() const;

  // Returns the last possible visual index to add an item view in the specified
  // page, used only for drag reordering.
  GridIndex GetLastTargetIndexOfPage(int page_index) const;

  // Returns the target model index if moving the item view to specified target
  // visual index.
  int GetTargetModelIndexForMove(AppListItemView* moved_view,
                                 const GridIndex& index) const;

  // Returns the target `AppsGridView::item_list_` index if moving the item view
  // to specified target visual index.
  int GetTargetItemListIndexForMove(AppListItemView* moved_view,
                                    const GridIndex& index) const;

  // Returns true if the visual index is valid position to which an item view
  // can be moved.
  bool IsValidReorderTargetIndex(const GridIndex& index) const;

  // Adds a page break at the end of |pages_|.
  void AppendPage();

  // Returns true if the page has no empty slot.
  bool IsFullPage(int page_index) const;

  // Returns total number of pages in the view structure.
  int total_pages() const { return pages_.size(); }

  // Returns total number of item views in the specified page.
  int items_on_page(int page_index) const {
    DCHECK_LT(page_index, total_pages());
    return pages_[page_index].size();
  }

  const Pages& pages() const { return pages_; }

 private:
  // Skips the item view being dragged if it exists in the specified
  // |page|.
  int CalculateTargetSlot(const Page& page) const;

  // Clear overflowing item views by moving them to the next page. Returns true
  // if view structure is changed.
  bool ClearOverflow();

  // Removes empty page. Returns true if view structure is changed.
  bool ClearEmptyPages();

  // Returns TilesPerPage() from `apps_grid_view_`.
  int TilesPerPage() const;

  // Not const for tests.
  Mode mode_ = Mode::kPartialPages;

  // Represents the item views' locations in each page.
  Pages pages_;

  AppsGridView* const apps_grid_view_;  // Not owned.
};

}  // namespace ash

#endif  // ASH_APP_LIST_PAGED_VIEW_STRUCTURE_H_
