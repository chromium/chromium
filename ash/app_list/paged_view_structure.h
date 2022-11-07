// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_PAGED_VIEW_STRUCTURE_H_
#define ASH_APP_LIST_PAGED_VIEW_STRUCTURE_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "base/check_op.h"

namespace ash {

class AppsGridView;
class AppListItem;
class AppListItemView;
struct GridIndex;

// Manages the mapping between AppListItemList index, view model index, and
// visual pages in the apps grid view.
class ASH_EXPORT PagedViewStructure {
 public:
  using Page = std::vector<AppListItemView*>;
  using Pages = std::vector<Page>;

  // Class that disables empty page or overflow sanitization when in scope.
  class ScopedSanitizeLock {
   public:
    explicit ScopedSanitizeLock(PagedViewStructure* view_structure);
    ScopedSanitizeLock(const ScopedSanitizeLock&) = delete;
    ScopedSanitizeLock& operator=(const ScopedSanitizeLock&) = delete;
    ~ScopedSanitizeLock();

   private:
    PagedViewStructure* const view_structure_;
  };

  explicit PagedViewStructure(AppsGridView* apps_grid_view);
  PagedViewStructure(const PagedViewStructure& other) = delete;
  PagedViewStructure& operator=(PagedViewStructure& other) = delete;
  ~PagedViewStructure();

  enum class Mode {
    // Paged, with all pages full. Used for paged apps grid (for tablet mode).
    kFullPages,
    // A single long page. Ignores page breaks in the data model. Used for
    // scrollable apps grid (for clamshell mode and folders).
    kSinglePage
  };
  void Init(Mode mode);

  // Temporarily disables sanitization of empty pages and page overflow. The
  // sanitization will remain disabled while the returned object remains in
  // scope. The paged view structure will be sanitized when the returned object
  // gets destroyed.
  // This should be used in cases where paged view structure has to be updated
  // in two or more steps, in which case view structure should only be sanitized
  // after the final step.
  // NOTE: The caller should ensure that the returned object does not outlive
  // the PagedViewStructure instance.
  std::unique_ptr<ScopedSanitizeLock> GetSanitizeLock();

  // Loads the view structure based on the position and page position in the
  // metadata of item views in the view model.
  void LoadFromMetadata();

  // Operations allowed to modify the view structure.
  void Move(AppListItemView* view, const GridIndex& target_index);
  void Remove(AppListItemView* view);
  void Add(AppListItemView* view, const GridIndex& target_index);

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
  int GetTargetModelIndexForMove(AppListItem* moved_item,
                                 const GridIndex& index) const;

  // Returns the target `AppsGridView::item_list_` index if moving the item view
  // to specified target visual index.
  int GetTargetItemListIndexForMove(AppListItem* moved_item,
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

  Mode mode() const { return mode_; }
  const Pages& pages() const { return pages_; }

 private:
  // Sanitizes the paged view structure - it clears page overflow and
  // removes empty pages. A sanitization step is skipped if any sanitization
  // disablers for that step are active.
  void Sanitize();

  // Clear overflowing item views by moving them to the next page.
  void ClearOverflow();

  // Removes empty pages.
  void ClearEmptyPages();

  // Returns TilesPerPage() from `apps_grid_view_`.
  int TilesPerPage(int page) const;

  // Not const for tests.
  Mode mode_ = Mode::kFullPages;

  // Represents the item views' locations in each page.
  Pages pages_;

  AppsGridView* const apps_grid_view_;  // Not owned.

  // The number of active `ScopedSanitizeLocks` that disable
  // sanitization of empty pages and page overflow sanitization.
  int sanitize_locks_ = 0;
};

}  // namespace ash

#endif  // ASH_APP_LIST_PAGED_VIEW_STRUCTURE_H_
