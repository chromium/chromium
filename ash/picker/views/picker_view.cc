// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/ash_element_identifiers.h"
#include "ash/picker/metrics/picker_session_metrics.h"
#include "ash/picker/model/picker_action_type.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/views/picker_emoji_bar_view.h"
#include "ash/picker/views/picker_key_event_handler.h"
#include "ash/picker/views/picker_main_container_view.h"
#include "ash/picker/views/picker_page_view.h"
#include "ash/picker/views/picker_pseudo_focus.h"
#include "ash/picker/views/picker_search_field_view.h"
#include "ash/picker/views/picker_search_results_view.h"
#include "ash/picker/views/picker_search_results_view_delegate.h"
#include "ash/picker/views/picker_strings.h"
#include "ash/picker/views/picker_style.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/picker/views/picker_zero_state_view.h"
#include "ash/public/cpp/picker/picker_category.h"
#include "ash/public/cpp/picker/picker_search_result.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chromeos/ash/resources/internal/strings/grit/ash_internal_strings.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

namespace ash {
namespace {

constexpr int kVerticalPaddingBetweenPickerContainers = 8;

// Padding to separate the Picker window from the screen edge.
constexpr gfx::Insets kPaddingFromScreenEdge(16);

std::unique_ptr<views::BubbleBorder> CreateBorder() {
  auto border = std::make_unique<views::BubbleBorder>(
      views::BubbleBorder::NONE, views::BubbleBorder::NO_SHADOW);
  border->SetCornerRadius(kPickerContainerBorderRadius);
  border->SetColor(SK_ColorTRANSPARENT);
  return border;
}

// Gets the preferred Picker view bounds in screen coordinates. We try to place
// the Picker view close to `anchor_bounds`, while taking into account
// `layout_type`, `picker_view_size` and available space on the screen.
// `picker_view_search_field_vertical_offset` is the vertical offset from the
// top of the Picker view to the center of the search field, which we use to try
// to vertically align the search field with the center of the anchor bounds.
// `anchor_bounds` and returned bounds should be in screen coordinates.
gfx::Rect GetPickerViewBounds(const gfx::Rect& anchor_bounds,
                              PickerLayoutType layout_type,
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
      case PickerLayoutType::kMainResultsBelowSearchField:
        // Try to place the Picker at the right edge of the screen, below the
        // anchor.
        picker_view_bounds.set_origin(
            {screen_work_area.right() - picker_view_size.width(),
             anchor_bounds.bottom()});
        break;
      case PickerLayoutType::kMainResultsAboveSearchField:
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
    case PickerSectionType::kNone:
    case PickerSectionType::kCategories:
    case PickerSectionType::kSuggestions:
    case PickerSectionType::kEditorWrite:
    case PickerSectionType::kEditorRewrite:
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

// TODO: b/331285414 - Finalize the search field placeholder text.
std::u16string GetSearchFieldPlaceholderText() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return l10n_util::GetStringUTF16(IDS_PICKER_SEARCH_FIELD_PLACEHOLDER_TEXT);
#else
  return l10n_util::GetStringUTF16(
      IDS_PICKER_ZERO_STATE_SEARCH_FIELD_PLACEHOLDER_TEXT);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

}  // namespace

PickerView::PickerView(PickerViewDelegate* delegate,
                       PickerLayoutType layout_type,
                       const base::TimeTicks trigger_event_timestamp)
    : performance_metrics_(trigger_event_timestamp), delegate_(delegate) {
  SetShowCloseButton(false);
  SetPreferredSize(kPickerViewMaxSize);
  SetProperty(views::kElementIdentifierKey, kPickerElementId);

  SetLayoutManager(std::make_unique<views::FlexLayout>())
      ->SetOrientation(views::LayoutOrientation::kVertical)
      .SetCollapseMargins(true)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets::VH(kVerticalPaddingBetweenPickerContainers, 0));

  AddMainContainerView(layout_type);
  if (base::Contains(delegate_->GetAvailableCategories(),
                     PickerCategory::kExpressions)) {
    AddEmojiBarView();
  }

  // Automatically focus on the search field.
  SetInitiallyFocusedView(search_field_view_);

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  key_event_handler_.SetActivePseudoFocusHandler(this);
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

void PickerView::SelectZeroStateCategory(PickerCategory category) {
  SelectCategory(category);
}

void PickerView::SelectZeroStateResult(const PickerSearchResult& result) {
  SelectSearchResult(result);
}

PickerActionType PickerView::GetActionForResult(
    const PickerSearchResult& result) {
  return delegate_->GetActionForResult(result);
}

void PickerView::GetZeroStateSuggestedResults(
    SuggestedResultsCallback callback) {
  delegate_->GetZeroStateSuggestedResults(std::move(callback));
}

void PickerView::RequestPseudoFocus(views::View* view) {
  if (active_item_container_ == nullptr ||
      !active_item_container_->ContainsItem(view)) {
    return;
  }
  SetPseudoFocusedView(view);
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
  } else if (const PickerSearchResult::EditorData* editor_data =
                 std::get_if<PickerSearchResult::EditorData>(&result.data())) {
    delegate_->ShowEditor(editor_data->preset_query_id,
                          editor_data->freeform_text);
  } else {
    delegate_->GetSessionMetrics().SetSelectedResult(
        result, search_results_view_->GetIndex(result));
    switch (delegate_->GetActionForResult(result)) {
      case PickerActionType::kInsert:
        delegate_->InsertResultOnNextFocus(result);
        GetWidget()->Close();
        break;
      case PickerActionType::kOpen:
      case PickerActionType::kDo:
        delegate_->OpenResult(result);
        GetWidget()->Close();
        break;
      case PickerActionType::kCreate:
        NOTREACHED_NORETURN();
    }
  }
}

void PickerView::SelectMoreResults(PickerSectionType type) {
  SelectCategoryWithQuery(GetCategoryForMoreResults(type),
                          search_field_view_->GetQueryText());
}

void PickerView::ShowEmojiPicker(ui::EmojiPickerCategory category) {
  PickerSessionMetrics& session_metrics = delegate_->GetSessionMetrics();
  session_metrics.SetSelectedCategory(PickerCategory::kExpressions);

  if (auto* widget = GetWidget()) {
    widget->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
  }

  session_metrics.SetOutcome(PickerSessionMetrics::SessionOutcome::kRedirected);
  delegate_->ShowEmojiPicker(category, search_field_view_->GetQueryText());
}

bool PickerView::DoPseudoFocusedAction() {
  return pseudo_focused_view_ == nullptr
             ? false
             : DoPickerPseudoFocusedActionOnView(pseudo_focused_view_);
}

bool PickerView::MovePseudoFocusUp() {
  if (views::View* item_above =
          active_item_container_->GetItemAbove(pseudo_focused_view_)) {
    SetPseudoFocusedView(item_above);
  } else {
    AdvanceActiveItemContainer(PickerPseudoFocusDirection::kBackward);
  }
  return true;
}

bool PickerView::MovePseudoFocusDown() {
  if (views::View* item_below =
          active_item_container_->GetItemBelow(pseudo_focused_view_)) {
    SetPseudoFocusedView(item_below);
  } else {
    AdvanceActiveItemContainer(PickerPseudoFocusDirection::kForward);
  }
  return true;
}

bool PickerView::MovePseudoFocusLeft() {
  if (views::View* left_item =
          active_item_container_->GetItemLeftOf(pseudo_focused_view_)) {
    SetPseudoFocusedView(left_item);
    return true;
  }
  return false;
}

bool PickerView::MovePseudoFocusRight() {
  if (views::View* right_item =
          active_item_container_->GetItemRightOf(pseudo_focused_view_)) {
    SetPseudoFocusedView(right_item);
    return true;
  }
  return false;
}

bool PickerView::AdvancePseudoFocus(PickerPseudoFocusDirection direction) {
  if (pseudo_focused_view_ == nullptr) {
    return false;
  }
  SetPseudoFocusedView(GetNextPickerPseudoFocusableView(
      pseudo_focused_view_, direction, /*should_loop=*/true));
  return true;
}

void PickerView::OnViewIsDeleting(View* observed_view) {
  CHECK_EQ(observed_view, pseudo_focused_view_);
  pseudo_focused_view_ = nullptr;
  pseudo_focused_view_observation_.Reset();
}

gfx::Rect PickerView::GetTargetBounds(const gfx::Rect& anchor_bounds,
                                      PickerLayoutType layout_type) {
  return GetPickerViewBounds(anchor_bounds, layout_type, size(),
                             search_field_view_->bounds().CenterPoint().y() +
                                 main_container_view_->bounds().y());
}

void PickerView::StartSearch(const std::u16string& query) {
  delegate_->GetSessionMetrics().UpdateSearchQuery(query);
  if (!query.empty()) {
    SetActivePage(search_results_view_);
    published_first_results_ = false;
    delegate_->StartEmojiSearch(query,
                                base::BindOnce(&PickerView::PublishEmojiResults,
                                               weak_ptr_factory_.GetWeakPtr()));
    delegate_->StartSearch(
        query, selected_category_,
        base::BindRepeating(
            &PickerView::PublishSearchResults, weak_ptr_factory_.GetWeakPtr(),
            /*show_no_results_found=*/selected_category_.has_value()));
  } else if (selected_category_.has_value()) {
    SetActivePage(category_results_view_);
  } else {
    search_results_view_->ClearSearchResults();
    ResetEmojiBarToZeroState();
    SetActivePage(zero_state_view_);
  }
}

void PickerView::PublishEmojiResults(std::vector<PickerSearchResult> results) {
  if (emoji_bar_view_ != nullptr) {
    emoji_bar_view_->SetSearchResults(std::move(results));
  }
}

void PickerView::PublishSearchResults(
    bool show_no_results_found,
    std::vector<PickerSearchResultsSection> results) {
  // TODO: b/333826943: This is a hacky way to detect if there are no results.
  // Design a better API for notifying when the search has completed without any
  // results.
  if (show_no_results_found && results.empty()) {
    search_results_view_->ShowNoResultsFound();
    return;
  }

  if (!published_first_results_) {
    search_results_view_->ClearSearchResults();
    published_first_results_ = true;
  }
  for (PickerSearchResultsSection& result : results) {
    // Do not show GIFs.
    if (result.type() == PickerSectionType::kGifs) {
      continue;
    } else {
      search_results_view_->AppendSearchResults(std::move(result));
    }
  }
  performance_metrics_.MarkSearchResultsUpdated();
}

void PickerView::SelectCategory(PickerCategory category) {
  SelectCategoryWithQuery(category, /*query=*/u"");
}

void PickerView::SelectCategoryWithQuery(PickerCategory category,
                                         std::u16string_view query) {
  PickerSessionMetrics& session_metrics = delegate_->GetSessionMetrics();
  session_metrics.SetSelectedCategory(category);
  selected_category_ = category;

  if (category == PickerCategory::kExpressions) {
    if (auto* widget = GetWidget()) {
      // TODO(b/316936394): Correctly handle opening of emoji picker. Probably
      // best to wait for the IME on focus event, or save some coordinates and
      // open emoji picker in the correct location in some other way.
      widget->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
    }
    session_metrics.SetOutcome(
        PickerSessionMetrics::SessionOutcome::kRedirected);
    delegate_->ShowEmojiPicker(ui::EmojiPickerCategory::kEmojis, query);
    return;
  }

  if (category == PickerCategory::kEditorWrite ||
      category == PickerCategory::kEditorRewrite) {
    if (auto* widget = GetWidget()) {
      // TODO: b/330267329 - Correctly handle opening of Editor. Probably
      // best to wait for the IME on focus event, or save some coordinates and
      // open Editor in the correct location in some other way.
      widget->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
    }
    CHECK(query.empty());
    session_metrics.SetOutcome(
        PickerSessionMetrics::SessionOutcome::kRedirected);
    delegate_->ShowEditor(/*preset_query_id*/ std::nullopt,
                          /*freeform_text=*/std::nullopt);
    return;
  }

  search_field_view_->SetPlaceholderText(
      GetSearchFieldPlaceholderTextForPickerCategory(category));
  search_field_view_->SetQueryText(std::u16string(query));
  search_field_view_->SetBackButtonVisible(true);

  if (query.empty()) {
    // Getting suggested results for a category can be slow, so show a loading
    // animation.
    category_results_view_->ShowLoadingAnimation();
    SetActivePage(category_results_view_);
    delegate_->GetResultsForCategory(
        category, base::BindRepeating(&PickerView::PublishCategoryResults,
                                      weak_ptr_factory_.GetWeakPtr()));
  } else {
    StartSearch(std::u16string(query));
  }
}

void PickerView::PublishCategoryResults(
    std::vector<PickerSearchResultsSection> results) {
  category_results_view_->ClearSearchResults();
  if (results.empty()) {
    category_results_view_->ShowNoResultsFound();
  } else {
    for (PickerSearchResultsSection& section : results) {
      category_results_view_->AppendSearchResults(std::move(section));
    }
  }
}

void PickerView::AddMainContainerView(PickerLayoutType layout_type) {
  main_container_view_ =
      AddChildView(std::make_unique<PickerMainContainerView>());

  // `base::Unretained` is safe here because this class owns
  // `main_container_view_`, which owns `search_field_view_`.
  search_field_view_ = main_container_view_->AddSearchFieldView(
      views::Builder<PickerSearchFieldView>(
          std::make_unique<PickerSearchFieldView>(
              base::BindRepeating(&PickerView::StartSearch,
                                  base::Unretained(this)),
              base::BindRepeating(&PickerView::OnSearchBackButtonPressed,
                                  base::Unretained(this)),
              &key_event_handler_, &performance_metrics_))
          .SetPlaceholderText(GetSearchFieldPlaceholderText())
          .Build());
  main_container_view_->AddContentsView(layout_type);

  zero_state_view_ =
      main_container_view_->AddPage(std::make_unique<PickerZeroStateView>(
          this, delegate_->GetAvailableCategories(), kPickerViewMaxSize.width(),
          delegate_->GetAssetFetcher(), &submenu_controller_));
  category_results_view_ =
      main_container_view_->AddPage(std::make_unique<PickerSearchResultsView>(
          this, kPickerViewMaxSize.width(), delegate_->GetAssetFetcher(),
          &submenu_controller_));
  search_results_view_ =
      main_container_view_->AddPage(std::make_unique<PickerSearchResultsView>(
          this, kPickerViewMaxSize.width(), delegate_->GetAssetFetcher(),
          &submenu_controller_));

  SetActivePage(zero_state_view_);
}

void PickerView::AddEmojiBarView() {
  emoji_bar_view_ = AddChildViewAt(
      std::make_unique<PickerEmojiBarView>(this, kPickerViewMaxSize.width()),
      0);
  ResetEmojiBarToZeroState();
}

void PickerView::SetActivePage(PickerPageView* page_view) {
  main_container_view_->SetActivePage(page_view);
  SetPseudoFocusedView(nullptr);
  active_item_container_ = page_view;
  SetPseudoFocusedView(active_item_container_->GetTopItem());
}

void PickerView::AdvanceActiveItemContainer(
    PickerPseudoFocusDirection direction) {
  if (emoji_bar_view_ == nullptr || active_item_container_ == emoji_bar_view_) {
    active_item_container_ = main_container_view_->active_page();
  } else {
    active_item_container_ = emoji_bar_view_;
  }
  SetPseudoFocusedView(direction == PickerPseudoFocusDirection::kForward
                           ? active_item_container_->GetTopItem()
                           : active_item_container_->GetBottomItem());
}

void PickerView::SetPseudoFocusedView(views::View* view) {
  if (pseudo_focused_view_ == view) {
    return;
  }

  if (emoji_bar_view_ != nullptr && emoji_bar_view_->Contains(view)) {
    active_item_container_ = emoji_bar_view_;
  } else {
    // TODO: b/347103427 - There are cases where `view` might not be in either
    // the emoji bar or the active page, e.g. the back button in the search
    // field. Properly handle these cases, e.g. by making the main container a
    // PickerTraversableItemContainer.
    active_item_container_ = main_container_view_->active_page();
  }

  RemovePickerPseudoFocusFromView(pseudo_focused_view_);
  pseudo_focused_view_observation_.Reset();
  pseudo_focused_view_ = view;
  search_field_view_->SetTextfieldActiveDescendant(view);

  if (view != nullptr) {
    pseudo_focused_view_observation_.Observe(view);
    view->ScrollViewToVisible();
    ApplyPickerPseudoFocusToView(view);
  }
}

void PickerView::OnSearchBackButtonPressed() {
  search_field_view_->SetPlaceholderText(GetSearchFieldPlaceholderText());
  search_field_view_->SetQueryText(u"");
  search_field_view_->SetBackButtonVisible(false);
  SetActivePage(zero_state_view_);
}

void PickerView::ResetEmojiBarToZeroState() {
  if (emoji_bar_view_ == nullptr) {
    return;
  }

  if (delegate_ == nullptr) {
    emoji_bar_view_->ClearSearchResults();
    return;
  }

  std::vector<PickerSearchResult> emoji_bar_results;
  std::vector<std::string> recent_emojis =
      delegate_->GetRecentEmoji(ui::EmojiPickerCategory::kEmojis);
  if (recent_emojis.empty()) {
    std::vector<std::string> placeholder_emojis =
        delegate_->GetPlaceholderEmojis();
    emoji_bar_results.reserve(placeholder_emojis.size());
    for (const std::string& emoji : placeholder_emojis) {
      emoji_bar_results.push_back(
          PickerSearchResult::Emoji(base::UTF8ToUTF16(emoji)));
    }
  } else {
    emoji_bar_results.reserve(recent_emojis.size());
    for (const std::string& emoji : recent_emojis) {
      emoji_bar_results.push_back(
          PickerSearchResult::Emoji(base::UTF8ToUTF16(emoji)));
    }
  }

  emoji_bar_view_->SetSearchResults(std::move(emoji_bar_results));
}

BEGIN_METADATA(PickerView)
END_METADATA

}  // namespace ash
