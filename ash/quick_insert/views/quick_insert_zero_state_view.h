// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_ZERO_STATE_VIEW_H_
#define ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_ZERO_STATE_VIEW_H_

#include <map>
#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/quick_insert/quick_insert_category.h"
#include "ash/quick_insert/quick_insert_search_result.h"
#include "ash/quick_insert/views/quick_insert_category_type.h"
#include "ash/quick_insert/views/quick_insert_page_view.h"
#include "ash/quick_insert/views/quick_insert_submenu_controller.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class View;
}

namespace ash {

class PickerAssetFetcher;
class PickerClipboardHistoryProvider;
class PickerPreviewBubbleController;
class PickerSectionListView;
class QuickInsertSectionView;
class PickerZeroStateViewDelegate;

class ASH_EXPORT PickerZeroStateView : public PickerPageView {
  METADATA_HEADER(PickerZeroStateView, PickerPageView)

 public:
  // `delegate`, `asset_fetcher`, `submenu_controller`, `preview_controller`
  // must remain valid for the lifetime of this class.
  explicit PickerZeroStateView(
      PickerZeroStateViewDelegate* delegate,
      base::span<const QuickInsertCategory> available_categories,
      int quick_insert_view_width,
      PickerAssetFetcher* asset_fetcher,
      PickerSubmenuController* submenu_controller,
      PickerPreviewBubbleController* preview_controller);
  PickerZeroStateView(const PickerZeroStateView&) = delete;
  PickerZeroStateView& operator=(const PickerZeroStateView&) = delete;
  ~PickerZeroStateView() override;

  // PickerPageView:
  views::View* GetTopItem() override;
  views::View* GetBottomItem() override;
  views::View* GetItemAbove(views::View* item) override;
  views::View* GetItemBelow(views::View* item) override;
  views::View* GetItemLeftOf(views::View* item) override;
  views::View* GetItemRightOf(views::View* item) override;
  bool ContainsItem(views::View* item) override;

  std::map<QuickInsertCategoryType, raw_ptr<QuickInsertSectionView>>
  category_section_views_for_testing() const {
    return category_section_views_;
  }

  QuickInsertSectionView* primary_section_view_for_testing() {
    return primary_section_view_;
  }

 private:
  void OnCategorySelected(QuickInsertCategory category);
  void OnResultSelected(const QuickInsertSearchResult& result);
  void RecordCapsLockIgnored(bool ignored);

  // Gets or creates the category type section for `category_type`.
  QuickInsertSectionView* GetOrCreateSectionView(
      QuickInsertCategoryType category_type);

  // Gets or creates the category type section to contain `category`.
  QuickInsertSectionView* GetOrCreateSectionView(QuickInsertCategory category);

  void OnFetchSuggestedResults(std::vector<QuickInsertSearchResult> result);
  void AddResultToSection(const QuickInsertSearchResult& result,
                          QuickInsertSectionView* section);

  raw_ptr<PickerZeroStateViewDelegate> delegate_;
  raw_ptr<PickerSubmenuController> submenu_controller_;
  raw_ptr<PickerPreviewBubbleController> preview_controller_;

  // The section list view, contains the section views.
  raw_ptr<PickerSectionListView> section_list_view_ = nullptr;

  // The primary section is a titleless section that is shown first.
  // It contains items such as zero-state suggestions.
  raw_ptr<QuickInsertSectionView> primary_section_view_ = nullptr;

  // Below the primary section, there is a set of sections for each category
  // type.
  std::map<QuickInsertCategoryType, raw_ptr<QuickInsertSectionView>>
      category_section_views_;

  std::unique_ptr<PickerClipboardHistoryProvider> clipboard_provider_;

  // Timer used to put caps lock toggle to the end of the primary section.
  base::OneShotTimer add_caps_lock_delay_timer_;

  base::WeakPtrFactory<PickerZeroStateView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_QUICK_INSERT_VIEWS_QUICK_INSERT_ZERO_STATE_VIEW_H_
