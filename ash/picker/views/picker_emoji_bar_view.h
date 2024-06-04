// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PICKER_VIEWS_PICKER_EMOJI_BAR_VIEW_H_
#define ASH_PICKER_VIEWS_PICKER_EMOJI_BAR_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/views/picker_pseudo_focus_handler.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class IconButton;
class PickerSearchResult;
class PickerSearchResultsViewDelegate;
class SystemShadow;

// View for the Picker emoji bar, which is a small bar above the main Picker
// container that shows expression search results (i.e. emojis, symbols and
// emoticons).
class ASH_EXPORT PickerEmojiBarView : public views::View,
                                      public PickerPseudoFocusHandler {
  METADATA_HEADER(PickerEmojiBarView, views::View)

 public:
  // `delegate` must remain valid for the lifetime of this class.
  PickerEmojiBarView(PickerSearchResultsViewDelegate* delegate,
                     int picker_view_width);
  PickerEmojiBarView(const PickerEmojiBarView&) = delete;
  PickerEmojiBarView& operator=(const PickerEmojiBarView&) = delete;
  ~PickerEmojiBarView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // PickerPseudoFocusHandler:
  bool DoPseudoFocusedAction() override;
  bool MovePseudoFocusUp() override;
  bool MovePseudoFocusDown() override;
  bool MovePseudoFocusLeft() override;
  bool MovePseudoFocusRight() override;
  bool AdvancePseudoFocus(PseudoFocusDirection direction) override;
  bool GainPseudoFocus(PseudoFocusDirection direction) override;
  void LosePseudoFocus() override;

  // Clears the emoji bar's search results.
  void ClearSearchResults();

  // Sets the results from `section` as the emoji bar's search results.
  void SetSearchResults(PickerSearchResultsSection section);

  views::View* item_row_for_testing() { return item_row_; }

  IconButton* more_emojis_button_for_testing() { return more_emojis_button_; }

 private:
  void SelectSearchResult(const PickerSearchResult& result);

  void OpenMoreEmojis();

  int CalculateAvailableWidthForItemRow();

  void SetPseudoFocusedView(views::View* view);

  std::unique_ptr<SystemShadow> shadow_;

  // `delegate_` outlives `this`.
  raw_ptr<PickerSearchResultsViewDelegate> delegate_;

  // The width of the PickerView that contains this emoji bar.
  int picker_view_width_ = 0;

  // Contains the item views corresponding to each search result.
  raw_ptr<views::View> item_row_ = nullptr;

  // The button for opening more emojis.
  raw_ptr<IconButton> more_emojis_button_ = nullptr;

  // The currently pseudo focused view, which responds to user actions that
  // trigger `DoPseudoFocusedAction`.
  raw_ptr<views::View> pseudo_focused_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_PICKER_VIEWS_PICKER_EMOJI_BAR_VIEW_H_
