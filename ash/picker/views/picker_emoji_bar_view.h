// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_EMOJI_BAR_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_EMOJI_BAR_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class PickerAssetFetcher;
class PickerSearchResult;
class PickerSearchResultsViewDelegate;
class PickerSectionView;
class SystemShadow;

// View for the Picker emoji bar, which is a small bar above the main Picker
// container that shows expression search results (i.e. emojis, symbols and
// emoticons).
class ASH_EXPORT PickerEmojiBarView : public views::View {
  METADATA_HEADER(PickerEmojiBarView, views::View)

 public:
  // `delegate` and `asset_fetcher` must remain valid for the lifetime of this
  // class.
  PickerEmojiBarView(PickerSearchResultsViewDelegate* delegate,
                     int picker_view_width,
                     PickerAssetFetcher* asset_fetcher);
  PickerEmojiBarView(const PickerEmojiBarView&) = delete;
  PickerEmojiBarView& operator=(const PickerEmojiBarView&) = delete;
  ~PickerEmojiBarView() override;

  // Clears the emoji bar's search results.
  void ClearSearchResults();

  // Sets the results from `section` as the emoji bar's search results.
  void SetSearchResults(PickerSearchResultsSection section);

  PickerSectionView* item_row_for_testing() { return item_row_; }

 private:
  void SelectSearchResult(const PickerSearchResult& result);

  std::unique_ptr<SystemShadow> shadow_;

  // `delegate_` outlives `this`.
  raw_ptr<PickerSearchResultsViewDelegate> delegate_;

  // Contains the item views corresponding to each search result.
  raw_ptr<PickerSectionView> item_row_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_EMOJI_BAR_VIEW_H_
