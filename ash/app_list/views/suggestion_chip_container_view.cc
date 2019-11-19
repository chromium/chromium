// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/suggestion_chip_container_view.h"

#include <memory>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "base/bind.h"
#include "base/callback.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// The spacing between chips.
constexpr int kChipSpacing = 8;

// The minimum allowed number of suggestion chips shown in the container
// (provided that the suggestoin chip results contain at least than number of
// items).
constexpr int kMinimumSuggestionChipNumber = 3;

bool IsPolicySuggestionChip(const SearchResult& result) {
  return result.display_location() ==
             SearchResultDisplayLocation::kSuggestionChipContainer &&
         result.display_index() != SearchResultDisplayIndex::kUndefined;
}

struct CompareByDisplayIndexAndThenPositionPriority {
  bool operator()(const SearchResult* result1,
                  const SearchResult* result2) const {
    // Sort increasing by display index, then decreasing by position priority.
    SearchResultDisplayIndex index1 = result1->display_index();
    SearchResultDisplayIndex index2 = result2->display_index();
    float priority1 = result1->position_priority();
    float priority2 = result2->position_priority();
    if (index1 != index2)
      return index1 < index2;
    return priority1 > priority2;
  }
};

}  // namespace

SuggestionChipContainerView::SuggestionChipContainerView(
    ContentsView* contents_view)
    : SearchResultContainerView(
          contents_view != nullptr
              ? contents_view->GetAppListMainView()->view_delegate()
              : nullptr),
      contents_view_(contents_view) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  DCHECK(contents_view);
  layout_manager_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets(), kChipSpacing));
  layout_manager_->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kCenter);

  for (size_t i = 0; i < static_cast<size_t>(
                             AppListConfig::instance().num_start_page_tiles());
       ++i) {
    SearchResultSuggestionChipView* chip =
        new SearchResultSuggestionChipView(view_delegate());
    chip->SetVisible(false);
    chip->set_index_in_container(i);
    suggestion_chip_views_.emplace_back(chip);
    AddChildView(chip);
  }
}

SuggestionChipContainerView::~SuggestionChipContainerView() = default;

SearchResultSuggestionChipView* SuggestionChipContainerView::GetResultViewAt(
    size_t index) {
  DCHECK(index >= 0 && index < suggestion_chip_views_.size());
  return suggestion_chip_views_[index];
}

int SuggestionChipContainerView::DoUpdate() {
  // Filter out priority suggestion chips with a non-default value
  // for |display_index|.
  auto filter_requested_index_chips = [](const SearchResult& r) -> bool {
    return IsPolicySuggestionChip(r);
  };
  std::vector<SearchResult*> requested_index_results =
      SearchModel::FilterSearchResultsByFunction(
          results(), base::BindRepeating(filter_requested_index_chips),
          AppListConfig::instance().num_start_page_tiles());

  std::sort(requested_index_results.begin(), requested_index_results.end(),
            CompareByDisplayIndexAndThenPositionPriority());

  // Handle placement issues that may arise when multiple app results have
  // the same requested index. Reassign the |display_index| of results
  // with lower priorities or with conflicting indexes so that the results are
  // added to the final display_results list in the correct order.
  int previous_index = -1;
  for (auto* result : requested_index_results) {
    int current_index = result->display_index();
    if (current_index <= previous_index) {
      current_index = previous_index + 1;
    }
    SearchResultDisplayIndex final_index =
        static_cast<SearchResultDisplayIndex>(current_index);
    result->set_display_index(final_index);
    previous_index = current_index;
  }

  // Need to filter out kArcAppShortcut since it will be confusing to users
  // if shortcuts are displayed as suggestion chips. Also filter out any
  // duplicate policy chip results.
  auto filter_reinstall_and_shortcut = [](const SearchResult& r) -> bool {
    return r.display_type() == SearchResultDisplayType::kRecommendation &&
           r.result_type() != AppListSearchResultType::kPlayStoreReinstallApp &&
           r.result_type() != AppListSearchResultType::kArcAppShortcut &&
           !IsPolicySuggestionChip(r);
  };
  std::vector<SearchResult*> display_results =
      SearchModel::FilterSearchResultsByFunction(
          results(), base::BindRepeating(filter_reinstall_and_shortcut),
          AppListConfig::instance().num_start_page_tiles() -
              requested_index_results.size());

  // Update display results list by placing policy result chips at their
  // specified |display_index|. Do not add with a |display_index| that is out
  // of bounds.
  for (auto* result : requested_index_results) {
    if (result->display_index() <=
        AppListConfig::instance().num_start_page_tiles() - 1) {
      display_results.emplace(display_results.begin() + result->display_index(),
                              result);
    }
  }

  // Update search results here, but wait until layout to add them as child
  // views when we know this view's bounds.
  for (size_t i = 0; i < static_cast<size_t>(
                             AppListConfig::instance().num_start_page_tiles());
       ++i) {
    suggestion_chip_views_[i]->SetResult(
        i < display_results.size() ? display_results[i] : nullptr);
  }

  Layout();
  return std::min(AppListConfig::instance().num_start_page_tiles(),
                  display_results.size());
}

const char* SuggestionChipContainerView::GetClassName() const {
  return "SuggestionChipContainerView";
}

void SuggestionChipContainerView::Layout() {
  // Only show the chips that fit in this view's contents bounds.
  int total_width = 0;
  const int max_width = GetContentsBounds().width();

  bool has_hidden_chip = false;
  std::vector<views::View*> shown_chips;
  for (auto* chip : suggestion_chip_views_) {
    layout_manager_->ClearFlexForView(chip);

    if (!chip->result()) {
      chip->SetVisible(false);
      continue;
    }

    const gfx::Size size = chip->GetPreferredSize();
    if (has_hidden_chip ||
        (size.width() + total_width > max_width &&
         shown_chips.size() >= kMinimumSuggestionChipNumber)) {
      chip->SetVisible(false);
      has_hidden_chip = true;
      continue;
    }

    chip->SetVisible(true);
    shown_chips.push_back(chip);

    total_width += (total_width == 0 ? 0 : kChipSpacing) + size.width();
  }

  // If current suggestion chip width is over the max value, reduce the width by
  // flexing views whose width is above average for the available space.
  if (total_width > max_width && shown_chips.size() > 0) {
    // Remove spacing between chips from total width to get the width available
    // to visible suggestion chip views.
    int available_width = std::max(
        0, max_width - (kMinimumSuggestionChipNumber - 1) * kChipSpacing);

    std::vector<views::View*> views_to_flex;
    views_to_flex.swap(shown_chips);

    // Do not flex views whose width is below average available width per chip,
    // as flexing those would actually increase their size. Repeat this until
    // there are no more views to remove from consideration for flexing
    // (removing a view increases the average available space for the remaining
    // views, so another view's size might fit into the remaining space).
    for (size_t i = 0; i < kMinimumSuggestionChipNumber - 1; ++i) {
      if (views_to_flex.empty())
        break;

      std::vector<views::View*> next_views_to_flex;
      const int avg_width = available_width / views_to_flex.size();
      for (auto* view : views_to_flex) {
        gfx::Size view_size = view->GetPreferredSize();
        if (view_size.width() <= avg_width) {
          available_width -= view_size.width();
        } else {
          next_views_to_flex.push_back(view);
        }
      }

      if (views_to_flex.size() == next_views_to_flex.size())
        break;
      views_to_flex.swap(next_views_to_flex);
    }

    // Flex the views that are left over.
    for (auto* view : views_to_flex)
      layout_manager_->SetFlexForView(view, 1);
  }

  views::View::Layout();
}

bool SuggestionChipContainerView::OnKeyPressed(const ui::KeyEvent& event) {
  // Let the FocusManager handle Left/Right keys.
  if (!IsUnhandledUpDownKeyEvent(event))
    return false;

  // Up key moves focus to the search box. Down key moves focus to the first
  // app.
  views::View* v = nullptr;
  if (event.key_code() == ui::VKEY_UP) {
    v = contents_view_->GetSearchBoxView()->search_box();
  } else {
    // The first app is the next to this view's last focusable view.
    views::View* last_focusable_view =
        GetFocusManager()->GetNextFocusableView(this, nullptr, true, false);
    v = GetFocusManager()->GetNextFocusableView(last_focusable_view, nullptr,
                                                false, false);
  }
  if (v)
    v->RequestFocus();
  return true;
}

void SuggestionChipContainerView::DisableFocusForShowingActiveFolder(
    bool disabled) {
  for (auto* chip : suggestion_chip_views_)
    chip->SetEnabled(!disabled);

  // Ignore the container view in accessibility tree so that suggestion chips
  // will not be accessed by ChromeVox.
  GetViewAccessibility().OverrideIsIgnored(disabled);
  GetViewAccessibility().NotifyAccessibilityEvent(
      ax::mojom::Event::kTreeChanged);
}

void SuggestionChipContainerView::OnTabletModeChanged(bool started) {
  // Enable/Disable chips' background blur based on tablet mode.
  for (auto* chip : suggestion_chip_views_)
    chip->SetBackgroundBlurEnabled(started);
}

}  // namespace ash
