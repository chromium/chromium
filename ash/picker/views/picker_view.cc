// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_view.h"

#include <memory>
#include <utility>

#include "ash/ash_element_identifiers.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/views/picker_category_view.h"
#include "ash/picker/views/picker_contents_view.h"
#include "ash/picker/views/picker_key_event_handler.h"
#include "ash/picker/views/picker_page_view.h"
#include "ash/picker/views/picker_search_field_view.h"
#include "ash/picker/views/picker_search_results_view.h"
#include "ash/picker/views/picker_strings.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/picker/views/picker_zero_state_view.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/style/system_shadow.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/background.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/highlight_border.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

namespace ash {
namespace {

constexpr gfx::Size kPickerSize(320, 340);
constexpr int kBorderRadius = 12;
constexpr SystemShadow::Type kShadowType = SystemShadow::Type::kElevation12;
constexpr ui::ColorId kBackgroundColor =
    cros_tokens::kCrosSysSystemBaseElevated;

// Padding to separate the Picker window from the screen edge.
constexpr gfx::Insets kPaddingFromScreenEdge(16);

std::unique_ptr<views::BubbleBorder> CreateBorder() {
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::NO_SHADOW);
  border->SetCornerRadius(kBorderRadius);
  return border;
}

std::unique_ptr<views::Separator> CreateSeparator() {
  return views::Builder<views::Separator>()
      .SetOrientation(views::Separator::Orientation::kHorizontal)
      .SetColorId(cros_tokens::kCrosSysSeparator)
      .Build();
}

// Gets the preferred Picker view bounds in screen coordinates. We try to place
// the Picker view close to `anchor_bounds`, while taking into account
// `layout_type`, `picker_view_size` and available space on the screen.
// `picker_view_search_field_vertical_offset` is the vertical offset from the
// top of the Picker view to the center of the search field, which we use to try
// to vertically align the search field with the center of the anchor bounds.
// `anchor_bounds` and returned bounds should be in screen coordinates.
gfx::Rect GetPickerViewBounds(const gfx::Rect& anchor_bounds,
                              PickerView::PickerLayoutType layout_type,
                              const gfx::Size& picker_view_size,
                              int picker_view_search_field_vertical_offset) {
  gfx::Rect screen_work_area = display::Screen::GetScreen()
                                   ->GetDisplayMatching(anchor_bounds)
                                   .work_area();
  screen_work_area.Inset(kPaddingFromScreenEdge);
  gfx::Rect picker_view_bounds(picker_view_size);
  if (anchor_bounds.right() + picker_view_size.width() <=
      screen_work_area.right()) {
    // If there is space, place the Picker to the right of the anchor,
    // vertically aligning the center of the Picker search field with the center
    // of the anchor.
    picker_view_bounds.set_origin(anchor_bounds.right_center());
    picker_view_bounds.Offset(0, -picker_view_search_field_vertical_offset);
  } else {
    switch (layout_type) {
      case PickerView::PickerLayoutType::kResultsBelowSearchField:
        // Try to place the Picker at the right edge of the screen, below the
        // anchor.
        picker_view_bounds.set_origin(
            {screen_work_area.right() - picker_view_size.width(),
             anchor_bounds.bottom()});
        break;
      case PickerView::PickerLayoutType::kResultsAboveSearchField:
        // Try to place the Picker at the right edge of the screen, above the
        // anchor.
        picker_view_bounds.set_origin(
            {screen_work_area.right() - picker_view_size.width(),
             anchor_bounds.y() - picker_view_size.height()});
        break;
    }
  }

  // Adjust if necessary to keep the whole Picker view onscreen. Note that the
  // non client area of the Picker, e.g. the shadows, are allowed to be
  // offscreen.
  picker_view_bounds.AdjustToFit(screen_work_area);
  return picker_view_bounds;
}

PickerCategory GetCategoryForMoreResults(PickerSectionType type) {
  switch (type) {
    case PickerSectionType::kCategories:
    case PickerSectionType::kSuggestions:
    case PickerSectionType::kRecentlyUsed:
    case PickerSectionType::kExamples:
      NOTREACHED_NORETURN();
    case PickerSectionType::kExpressions:
      return PickerCategory::kExpressions;
    case PickerSectionType::kLinks:
      return PickerCategory::kLinks;
    case PickerSectionType::kFiles:
      return PickerCategory::kLocalFiles;
    case PickerSectionType::kDriveFiles:
      return PickerCategory::kDriveFiles;
    case PickerSectionType::kGifs:
      return PickerCategory::kExpressions;
  }
}

}  // namespace

PickerView::PickerView(PickerViewDelegate* delegate,
                       PickerLayoutType layout_type,
                       const base::TimeTicks trigger_event_timestamp)
    : performance_metrics_(trigger_event_timestamp), delegate_(delegate) {
  SetShowCloseButton(false);
  SetBackground(views::CreateThemedRoundedRectBackground(kBackgroundColor,
                                                         kBorderRadius));
  SetBorder(std::make_unique<views::HighlightBorder>(
      kBorderRadius, views::HighlightBorder::Type::kHighlightBorderOnShadow));
  shadow_ =
      SystemShadow::CreateShadowOnNinePatchLayerForView(this, kShadowType);
  shadow_->SetRoundedCornerRadius(kBorderRadius);
  SetPreferredSize(kPickerSize);
  SetProperty(views::kElementIdentifierKey, kPickerElementId);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical);

  switch (layout_type) {
    case PickerLayoutType::kResultsBelowSearchField:
      AddSearchFieldView();
      AddChildView(CreateSeparator());
      AddContentsView(layout_type);
      break;
    case PickerLayoutType::kResultsAboveSearchField:
      AddContentsView(layout_type);
      AddChildView(CreateSeparator());
      AddSearchFieldView();
      break;
  }

  // Automatically focus on the search field.
  SetInitiallyFocusedView(search_field_view_);

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
}

PickerView::~PickerView() = default;

bool PickerView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  CHECK_EQ(accelerator.key_code(), ui::VKEY_ESCAPE);
  if (auto* widget = GetWidget()) {
    widget->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
  }
  return true;
}

std::unique_ptr<views::NonClientFrameView> PickerView::CreateNonClientFrameView(
    views::Widget* widget) {
  auto frame =
      std::make_unique<views::BubbleFrameView>(gfx::Insets(), gfx::Insets());
  frame->SetBubbleBorder(CreateBorder());
  return frame;
}

void PickerView::AddedToWidget() {
  performance_metrics_.StartRecording(*GetWidget());
}

void PickerView::RemovedFromWidget() {
  performance_metrics_.StopRecording();
}

gfx::Rect PickerView::GetTargetBounds(const gfx::Rect& anchor_bounds,
                                      PickerLayoutType layout_type) {
  return GetPickerViewBounds(anchor_bounds, layout_type, size(),
                             search_field_view_->bounds().CenterPoint().y());
}

void PickerView::OnSelectMoreResults(PickerSectionType type) {
  SelectCategoryWithQuery(GetCategoryForMoreResults(type),
                          search_field_view_->GetQueryText());
}

void PickerView::StartSearch(const std::u16string& query) {
  if (!query.empty()) {
    SetActivePage(search_results_view_);
    published_first_results_ = false;
    delegate_->StartSearch(
        query, selected_category_,
        base::BindRepeating(&PickerView::PublishSearchResults,
                            weak_ptr_factory_.GetWeakPtr()));
  } else if (selected_category_.has_value()) {
    SetActivePage(category_view_);
  } else {
    search_results_view_->ClearSearchResults();
    SetActivePage(zero_state_view_);
  }
}

void PickerView::PublishSearchResults(
    std::vector<PickerSearchResultsSection> results) {
  if (!published_first_results_) {
    search_results_view_->ClearSearchResults();
    published_first_results_ = true;
  }
  for (PickerSearchResultsSection& result : results) {
    search_results_view_->AppendSearchResults(std::move(result));
  }
  performance_metrics_.MarkSearchResultsUpdated();
}

void PickerView::SelectSearchResult(const PickerSearchResult& result) {
  if (const PickerSearchResult::CategoryData* category_data =
          std::get_if<PickerSearchResult::CategoryData>(&result.data())) {
    SelectCategory(category_data->category);
  } else if (const PickerSearchResult::SearchRequestData* search_request_data =
                 std::get_if<PickerSearchResult::SearchRequestData>(
                     &result.data())) {
    search_field_view_->SetQueryText(search_request_data->text);
    StartSearch(search_request_data->text);
  } else {
    delegate_->InsertResultOnNextFocus(result);
    GetWidget()->Close();
  }
}

void PickerView::SelectCategory(PickerCategory category) {
  SelectCategoryWithQuery(category, /*query=*/u"");
}

void PickerView::SelectCategoryWithQuery(PickerCategory category,
                                         std::u16string_view query) {
  selected_category_ = category;

  if (category == PickerCategory::kExpressions) {
    if (auto* widget = GetWidget()) {
      // TODO(b/316936394): Correctly handle opening of emoji picker. Probably
      // best to wait for the IME on focus event, or save some coordinates and
      // open emoji picker in the correct location in some other way.
      widget->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
    }
    delegate_->ShowEmojiPicker(ui::EmojiPickerCategory::kEmojis);
    return;
  }

  if (category == PickerCategory::kEditor) {
    if (auto* widget = GetWidget()) {
      // TODO: b/330267329 - Correctly handle opening of Editor. Probably
      // best to wait for the IME on focus event, or save some coordinates and
      // open Editor in the correct location in some other way.
      widget->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
    }
    delegate_->ShowEditor();
    return;
  }

  if (GetPickerCategoryType(category) ==
      PickerCategoryType::kCaseTransformations) {
    delegate_->TransformSelectedText(category);
    GetWidget()->Close();
    return;
  }

  if (category == PickerCategory::kCapsOn ||
      category == PickerCategory::kCapsOff) {
    GetWidget()->Close();
    delegate_->SetCapsLockEnabled(category == PickerCategory::kCapsOn);
    return;
  }

  search_field_view_->SetPlaceholderText(
      GetSearchFieldPlaceholderTextForPickerCategory(category));
  search_field_view_->SetQueryText(std::u16string(query));

  if (query.empty()) {
    // Getting suggested results for a category can be slow, so show a loading
    // animation.
    category_view_->ShowLoadingAnimation();
    SetActivePage(category_view_);
    delegate_->GetResultsForCategory(
        category, base::BindRepeating(&PickerView::PublishCategoryResults,
                                      weak_ptr_factory_.GetWeakPtr()));
  } else {
    StartSearch(std::u16string(query));
  }
}

void PickerView::PublishCategoryResults(
    std::vector<PickerSearchResultsSection> results) {
  category_view_->SetResults(std::move(results));
}

void PickerView::AddSearchFieldView() {
  // `base::Unretained` is safe here because this class owns
  // `search_field_view_`.
  search_field_view_ = AddChildView(std::make_unique<PickerSearchFieldView>(
      base::BindRepeating(&PickerView::StartSearch, base::Unretained(this)),
      &key_event_handler_, &performance_metrics_));
}

void PickerView::AddContentsView(PickerLayoutType layout_type) {
  contents_view_ =
      AddChildView(std::make_unique<PickerContentsView>(layout_type));
  contents_view_->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded)
          .WithWeight(1));

  // `base::Unretained` is safe here because this class owns
  // `zero_state_view_`, `category_view_` and `search_results_view`_.
  zero_state_view_ =
      contents_view_->AddPage(std::make_unique<PickerZeroStateView>(
          delegate_->GetAvailableCategories(),
          delegate_->ShouldShowSuggestedResults(), kPickerSize.width(),
          base::BindRepeating(&PickerView::SelectCategory,
                              base::Unretained(this)),
          base::BindRepeating(&PickerView::SelectSearchResult,
                              base::Unretained(this))));
  category_view_ = contents_view_->AddPage(std::make_unique<PickerCategoryView>(
      kPickerSize.width(),
      base::BindOnce(&PickerView::SelectSearchResult, base::Unretained(this)),
      delegate_->GetAssetFetcher()));
  search_results_view_ =
      contents_view_->AddPage(std::make_unique<PickerSearchResultsView>(
          kPickerSize.width(),
          base::BindOnce(&PickerView::SelectSearchResult,
                         base::Unretained(this)),
          base::BindRepeating(&PickerView::OnSelectMoreResults,
                              base::Unretained(this)),
          delegate_->GetAssetFetcher()));
  SetActivePage(zero_state_view_);
}

void PickerView::SetActivePage(PickerPageView* page_view) {
  contents_view_->SetActivePage(page_view);
  key_event_handler_.SetActivePseudoFocusHandler(page_view);
}

BEGIN_METADATA(PickerView)
END_METADATA

}  // namespace ash
