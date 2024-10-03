// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_view.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ash/ash_element_identifiers.h"
#include "ash/constants/ash_features.h"
#include "ash/picker/metrics/picker_performance_metrics.h"
#include "ash/picker/metrics/picker_session_metrics.h"
#include "ash/picker/model/picker_action_type.h"
#include "ash/picker/model/picker_caps_lock_position.h"
#include "ash/picker/model/picker_mode_type.h"
#include "ash/picker/model/picker_search_results_section.h"
#include "ash/picker/picker_category.h"
#include "ash/picker/picker_search_result.h"
#include "ash/picker/views/picker_emoji_bar_view.h"
#include "ash/picker/views/picker_item_with_submenu_view.h"
#include "ash/picker/views/picker_key_event_handler.h"
#include "ash/picker/views/picker_main_container_view.h"
#include "ash/picker/views/picker_page_view.h"
#include "ash/picker/views/picker_positioning.h"
#include "ash/picker/views/picker_pseudo_focus.h"
#include "ash/picker/views/picker_search_bar_textfield.h"
#include "ash/picker/views/picker_search_field_view.h"
#include "ash/picker/views/picker_search_results_view.h"
#include "ash/picker/views/picker_search_results_view_delegate.h"
#include "ash/picker/views/picker_strings.h"
#include "ash/picker/views/picker_style.h"
#include "ash/picker/views/picker_submenu_controller.h"
#include "ash/picker/views/picker_submenu_view.h"
#include "ash/picker/views/picker_traversable_item_container.h"
#include "ash/picker/views/picker_view_delegate.h"
#include "ash/picker/views/picker_zero_state_view.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/branding_buildflags.h"
#include "chromeos/ash/grit/ash_resources.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/emoji/emoji_panel_helper.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/resource/resource_bundle.h"
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
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_tracker.h"
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
gfx::Rect GetPickerViewBoundsWithoutSelectedText(
    const gfx::Rect& anchor_bounds,
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

// Gets the preferred Picker view bounds in the case that there is selected
// text. We try to left align the Picker view above or below `anchor_bounds`,
// while taking into account `layout_type`, `picker_view_size` and available
// space on the screen. `anchor_bounds` and returned bounds should be in screen
// coordinates.
gfx::Rect GetPickerViewBoundsWithSelectedText(
    const gfx::Rect& anchor_bounds,
    PickerLayoutType layout_type,
    const gfx::Size& picker_view_size) {
  gfx::Rect screen_work_area = display::Screen::GetScreen()
                                   ->GetDisplayMatching(anchor_bounds)
                                   .work_area();
  screen_work_area.Inset(kPaddingFromScreenEdge);
  gfx::Rect picker_view_bounds(picker_view_size);
  switch (layout_type) {
    case PickerLayoutType::kMainResultsBelowSearchField:
      // Left aligned below the anchor.
      picker_view_bounds.set_origin(
          gfx::Point(anchor_bounds.x(), anchor_bounds.bottom()));
      break;
    case PickerLayoutType::kMainResultsAboveSearchField:
      // Left aligned above the anchor.
      picker_view_bounds.set_origin(gfx::Point(
          anchor_bounds.x(), anchor_bounds.y() - picker_view_size.height()));
      break;
  }

  // Adjust if necessary to keep the whole Picker view onscreen.
  picker_view_bounds.AdjustToFit(screen_work_area);
  return picker_view_bounds;
}

PickerCategory GetCategoryForMoreResults(PickerSectionType type) {
  switch (type) {
    case PickerSectionType::kNone:
    case PickerSectionType::kContentEditor:
    case PickerSectionType::kExamples:
      NOTREACHED_NORETURN();
    case PickerSectionType::kClipboard:
      return PickerCategory::kClipboard;
    case PickerSectionType::kLinks:
      return PickerCategory::kLinks;
    case PickerSectionType::kLocalFiles:
      return PickerCategory::kLocalFiles;
    case PickerSectionType::kDriveFiles:
      return PickerCategory::kDriveFiles;
  }
}

std::u16string GetSearchFieldPlaceholderText(PickerModeType mode,
                                             bool is_editor_available) {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  switch (mode) {
    case PickerModeType::kUnfocused:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_SEARCH_FIELD_NO_FOCUS_PLACEHOLDER_TEXT);
    case PickerModeType::kNoSelection:
      return l10n_util::GetStringUTF16(
          is_editor_available
              ? IDS_PICKER_SEARCH_FIELD_NO_SELECTION_WITH_EDITOR_PLACEHOLDER_TEXT
              : IDS_PICKER_SEARCH_FIELD_NO_SELECTION_PLACEHOLDER_TEXT);
    case PickerModeType::kHasSelection:
      return l10n_util::GetStringUTF16(
          is_editor_available
              ? IDS_PICKER_SEARCH_FIELD_HAS_SELECTION_WITH_EDITOR_PLACEHOLDER_TEXT
              : IDS_PICKER_SEARCH_FIELD_HAS_SELECTION_PLACEHOLDER_TEXT);
    default:
      NOTREACHED_NORETURN();
  }
#else
  return u"Placeholder";
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
}

std::u16string GetNoResultsFoundDescription(PickerCategory category) {
  switch (category) {
    case PickerCategory::kLinks:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_NO_RESULTS_FOR_BROWSING_HISTORY_LABEL_TEXT);
    case PickerCategory::kClipboard:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_NO_RESULTS_FOR_CLIPBOARD_LABEL_TEXT);
    case PickerCategory::kDriveFiles:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_NO_RESULTS_FOR_DRIVE_FILES_LABEL_TEXT);
    case PickerCategory::kLocalFiles:
      return l10n_util::GetStringUTF16(
          IDS_PICKER_NO_RESULTS_FOR_LOCAL_FILES_LABEL_TEXT);
    case PickerCategory::kDatesTimes:
    case PickerCategory::kUnitsMaths:
      // TODO: b/345303965 - Add finalized strings for dates and maths.
      return l10n_util::GetStringUTF16(IDS_PICKER_NO_RESULTS_TEXT);
    case PickerCategory::kEditorWrite:
    case PickerCategory::kEditorRewrite:
    case PickerCategory::kLobster:
    case PickerCategory::kEmojisGifs:
    case PickerCategory::kEmojis:
      NOTREACHED_NORETURN();
  }
}

ui::ImageModel GetNoResultsFoundIllustration() {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return ui::ResourceBundle::GetSharedInstance().GetThemedLottieImageNamed(
      IDR_PICKER_NO_RESULTS_ILLUSTRATION);
#else
  return {};
#endif
}

bool IsEditorAvailable(base::span<PickerCategory> available_categories) {
  return base::Contains(available_categories, PickerCategory::kEditorWrite) ||
         base::Contains(available_categories, PickerCategory::kEditorRewrite);
}

}  // namespace

PickerView::PickerView(PickerViewDelegate* delegate,
                       const gfx::Rect& anchor_bounds,
                       PickerLayoutType layout_type,
                       PickerPositionType position_type,
                       const base::TimeTicks trigger_event_timestamp)
    : performance_metrics_(trigger_event_timestamp), delegate_(delegate) {
  SetShowCloseButton(false);
  SetProperty(views::kElementIdentifierKey, kPickerElementId);
  // TODO: b/357991165 - The desired bounds delegate here is *not* used directly
  // by the widget, because PickerWidget does not use `autosize`. Rather,
  // PickerView manually calls GetDesiredWidgetBounds to adjust the Widget
  // bounds to realign the search field with the caret position. Move this logic
  // to a standalone class.
  if (position_type == PickerPositionType::kNearAnchor) {
    set_desired_bounds_delegate(base::BindRepeating(
        &PickerView::GetTargetBounds, base::Unretained(this), anchor_bounds,
        layout_type));
  }

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::LayoutOrientation::kVertical,
      /*inside_border_insets=*/gfx::Insets(),
      /*between_child_spacing=*/kVerticalPaddingBetweenPickerContainers));

  AddMainContainerView(layout_type);
  if (base::Contains(delegate_->GetAvailableCategories(),
                     PickerCategory::kEmojisGifs) ||
      base::Contains(delegate_->GetAvailableCategories(),
                     PickerCategory::kEmojis)) {
    AddEmojiBarView();
  }

  // Automatically focus on the search field.
  SetInitiallyFocusedView(search_field_view_);

  AddAccelerator(ui::Accelerator(ui::VKEY_ESCAPE, ui::EF_NONE));
  AddAccelerator(ui::Accelerator(ui::VKEY_BROWSER_BACK, ui::EF_NONE));
  key_event_handler_.SetActivePseudoFocusHandler(this);

  pseudo_focused_view_tracker_.SetTrackEntireViewHierarchy(true);
  preview_bubble_observation_.Observe(&preview_controller_);
}

PickerView::~PickerView() = default;

bool PickerView::AcceleratorPressed(const ui::Accelerator& accelerator) {
  switch (accelerator.key_code()) {
    case ui::VKEY_ESCAPE:
      if (preview_controller_.IsBubbleVisible()) {
        preview_controller_.CloseBubble();
      } else if (submenu_controller_.GetSubmenuView() != nullptr) {
        submenu_controller_.Close();
      } else if (auto* widget = GetWidget()) {
        // Otherwise, close the Picker widget.
        widget->CloseWithReason(views::Widget::ClosedReason::kEscKeyPressed);
      }
      return true;
    case ui::VKEY_BROWSER_BACK:
      OnSearchBackButtonPressed();
      return true;
    default:
      NOTREACHED_NORETURN();
  }
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
  // Due to layout considerations, only populate the emoji bar after the
  // PickerView has been added to a widget.
  ResetEmojiBarToZeroState();
}

void PickerView::RemovedFromWidget() {
  performance_metrics_.StopRecording();
}

void PickerView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  if (widget_bounds_needs_update_ && GetWidget() != nullptr) {
    GetWidget()->SetBounds(GetDesiredWidgetBounds());
    widget_bounds_needs_update_ = false;
  }
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

void PickerView::OnSearchResultsViewHeightChanged() {
  SetWidgetBoundsNeedsUpdate();
}

void PickerView::GetZeroStateSuggestedResults(
    SuggestedResultsCallback callback) {
  delegate_->GetZeroStateSuggestedResults(std::move(callback));
}

void PickerView::RequestPseudoFocus(views::View* view) {
  // Only allow `view` to become pseudo focused if it is visible and part of the
  // active item container.
  if (view == nullptr || !view->IsDrawn() ||
      active_item_container_ == nullptr ||
      !active_item_container_->ContainsItem(view)) {
    return;
  }
  SetPseudoFocusedView(view);
}

void PickerView::OnZeroStateViewHeightChanged() {
  SetWidgetBoundsNeedsUpdate();
}

PickerCapsLockPosition PickerView::GetCapsLockPosition() {
  return delegate_->GetCapsLockPosition();
}

void PickerView::SetCapsLockDisplayed(bool displayed) {
  delegate_->GetSessionMetrics().SetCapsLockDisplayed(displayed);
}

void PickerView::SelectSearchResult(const PickerSearchResult& result) {
  if (const PickerCategoryResult* category_data =
          std::get_if<PickerCategoryResult>(&result)) {
    SelectCategory(category_data->category);
  } else if (const PickerSearchRequestResult* search_request_data =
                 std::get_if<PickerSearchRequestResult>(&result)) {
    UpdateSearchQueryAndActivePage(search_request_data->primary_text);
  } else if (const PickerEditorResult* editor_data =
                 std::get_if<PickerEditorResult>(&result)) {
    delegate_->ShowEditor(
        editor_data->preset_query_id,
        base::UTF16ToUTF8(search_field_view_->GetQueryText()));
  } else if (std::get_if<PickerLobsterResult>(&result)) {
    delegate_->ShowLobster(
        base::UTF16ToUTF8(search_field_view_->GetQueryText()));
  } else {
    delegate_->GetSessionMetrics().SetSelectedResult(
        result, search_results_view_->GetIndex(result));
    switch (delegate_->GetActionForResult(result)) {
      case PickerActionType::kInsert:
        delegate_->CloseWidgetThenInsertResultOnNextFocus(result);
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

void PickerView::ToggleGifs() {
  ShowEmojiPicker(ui::EmojiPickerCategory::kGifs);
}

void PickerView::ShowEmojiPicker(ui::EmojiPickerCategory category) {
  PickerSessionMetrics& session_metrics = delegate_->GetSessionMetrics();
  session_metrics.SetSelectedCategory(PickerCategory::kEmojisGifs);

  if (auto* widget = GetWidget()) {
    widget->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
  }

  session_metrics.SetOutcome(PickerSessionMetrics::SessionOutcome::kRedirected);
  delegate_->ShowEmojiPicker(category, search_field_view_->GetQueryText());
}

bool PickerView::DoPseudoFocusedAction() {
  if (clear_results_timer_.IsRunning()) {
    // New results are still pending.
    // TODO: b/351920494 - Insert the first new result instead of doing nothing.
    return false;
  }

  if (auto* submenu_view = views::AsViewClass<PickerItemWithSubmenuView>(
          GetPseudoFocusedView())) {
    submenu_view->ShowSubmenu();
    SetPseudoFocusedView(submenu_controller_.GetSubmenuView()->GetTopItem());
    return true;
  }

  return GetPseudoFocusedView() == nullptr
             ? false
             : DoPickerPseudoFocusedActionOnView(GetPseudoFocusedView());
}

bool PickerView::MovePseudoFocusUp() {
  if (views::View* item_above =
          active_item_container_->GetItemAbove(GetPseudoFocusedView())) {
    SetPseudoFocusedView(item_above);
  } else {
    AdvanceActiveItemContainer(PickerPseudoFocusDirection::kBackward);
  }
  return true;
}

bool PickerView::MovePseudoFocusDown() {
  if (views::View* item_below =
          active_item_container_->GetItemBelow(GetPseudoFocusedView())) {
    SetPseudoFocusedView(item_below);
  } else {
    AdvanceActiveItemContainer(PickerPseudoFocusDirection::kForward);
  }
  return true;
}

bool PickerView::MovePseudoFocusLeft() {
  views::View* pseudo_focused_view = GetPseudoFocusedView();
  if (IsContainedInSubmenu(pseudo_focused_view)) {
    SetPseudoFocusedView(submenu_controller_.GetAnchorView());
    submenu_controller_.Close();
    return true;
  }

  if (search_field_view_->Contains(pseudo_focused_view)) {
    if (search_field_view_->LeftEventShouldMoveCursor(pseudo_focused_view)) {
      return false;
    }
    views::View* left_view =
        search_field_view_->GetViewLeftOf(pseudo_focused_view);
    SetPseudoFocusedView(left_view);
    search_field_view_->OnGainedPseudoFocusFromLeftEvent(left_view);
    return true;
  }

  if (views::View* left_item =
          active_item_container_->GetItemLeftOf(pseudo_focused_view)) {
    SetPseudoFocusedView(left_item);
    return true;
  }
  return false;
}

bool PickerView::MovePseudoFocusRight() {
  views::View* pseudo_focused_view = GetPseudoFocusedView();
  if (views::IsViewClass<PickerItemWithSubmenuView>(pseudo_focused_view)) {
    views::AsViewClass<PickerItemWithSubmenuView>(pseudo_focused_view)
        ->ShowSubmenu();
    SetPseudoFocusedView(submenu_controller_.GetSubmenuView()->GetTopItem());
    return true;
  }

  if (search_field_view_->Contains(pseudo_focused_view)) {
    if (search_field_view_->RightEventShouldMoveCursor(pseudo_focused_view)) {
      return false;
    }
    views::View* right_view =
        search_field_view_->GetViewRightOf(pseudo_focused_view);
    SetPseudoFocusedView(right_view);
    search_field_view_->OnGainedPseudoFocusFromRightEvent(right_view);
    return true;
  }

  if (views::View* right_item =
          active_item_container_->GetItemRightOf(pseudo_focused_view)) {
    SetPseudoFocusedView(right_item);
    return true;
  }
  return false;
}

bool PickerView::AdvancePseudoFocus(PickerPseudoFocusDirection direction) {
  if (preview_controller_.IsBubbleVisible()) {
    preview_controller_.CloseBubble();
  }
  if (GetPseudoFocusedView() == nullptr) {
    return false;
  }
  SetPseudoFocusedView(GetNextPickerPseudoFocusableView(
      GetPseudoFocusedView(), direction, /*should_loop=*/true));
  return true;
}

void PickerView::OnPreviewBubbleVisibilityChanged(bool visible) {
  if (views::Widget* widget = GetWidget()) {
    // When the bubble is visible, turn off hiding the cursor on Esc key.
    // If the cursor hides on Esc, the preview bubble is closed due to its
    // OnMouseExit event handler, before PickerView has a chance to handle the
    // Esc key.
    widget->GetNativeWindow()->SetProperty(ash::kShowCursorOnKeypress, visible);
  }
}

gfx::Rect PickerView::GetTargetBounds(const gfx::Rect& anchor_bounds,
                                      PickerLayoutType layout_type) {
  return delegate_->GetMode() == PickerModeType::kHasSelection
             ? GetPickerViewBoundsWithSelectedText(anchor_bounds, layout_type,
                                                   size())
             : GetPickerViewBoundsWithoutSelectedText(
                   anchor_bounds, layout_type, size(),
                   search_field_view_->bounds().CenterPoint().y() +
                       main_container_view_->bounds().y());
}

void PickerView::UpdateSearchQueryAndActivePage(std::u16string query) {
  search_field_view_->SetQueryText(std::move(query));
  search_field_view_->RequestFocus();
  UpdateActivePage();
}

void PickerView::UpdateActivePage() {
  std::u16string_view query =
      base::TrimWhitespace(search_field_view_->GetQueryText(), base::TRIM_ALL);

  if (query == last_query_ && selected_category_ == last_selected_category_) {
    return;
  }
  last_query_ = std::u16string(query);
  last_selected_category_ = selected_category_;

  delegate_->GetSessionMetrics().UpdateSearchQuery(query);

  if (!query.empty()) {
    // Don't switch the active page immediately to the search view - this will
    // be done when the clear results timer fires, or when
    // `PublishSearchResults` is called.
    clear_results_timer_.Start(
        FROM_HERE, kClearResultsTimeout,
        base::BindOnce(&PickerView::OnClearResultsTimerFired,
                       weak_ptr_factory_.GetWeakPtr()));
    delegate_->StartEmojiSearch(query,
                                base::BindOnce(&PickerView::PublishEmojiResults,
                                               weak_ptr_factory_.GetWeakPtr()));
    delegate_->StartSearch(
        query, selected_category_,
        base::BindRepeating(&PickerView::PublishSearchResults,
                            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (selected_category_.has_value()) {
    SetActivePage(category_results_view_);
    if (last_suggested_results_category_ != selected_category_) {
      // Getting suggested results for a category can be slow, so show a
      // loading animation.
      category_results_view_->ShowLoadingAnimation();
      delegate_->GetResultsForCategory(
          *selected_category_,
          base::BindRepeating(&PickerView::PublishCategoryResults,
                              weak_ptr_factory_.GetWeakPtr(),
                              *selected_category_));
      last_suggested_results_category_ = selected_category_;
    }
  } else {
    SetActivePage(zero_state_view_);
  }
  delegate_->StopSearch();
  clear_results_timer_.Stop();
  search_results_view_->ClearSearchResults();
  ResetEmojiBarToZeroState();
}

void PickerView::PublishEmojiResults(std::vector<PickerEmojiResult> results) {
  if (emoji_bar_view_ == nullptr) {
    return;
  }

  emoji_bar_view_->SetSearchResults(std::move(results));
  search_results_view_->SetNumEmojiResultsForA11y(
      emoji_bar_view_->GetNumItems());
}

void PickerView::OnClearResultsTimerFired() {
  // `PickerView::UpdateActivePage` ensures that if the active page was set to
  // the zero state or category view, the timer that this is called from is
  // cancelled - which guarantees that this can't be called.
  SetActivePage(search_results_view_);

  search_results_view_->ClearSearchResults();
  performance_metrics_.MarkSearchResultsUpdated(
      PickerPerformanceMetrics::SearchResultsUpdate::kClear);
}

void PickerView::PublishSearchResults(
    std::vector<PickerSearchResultsSection> results) {
  // `PickerView::UpdateActivePage` ensures that if the active page was set to
  // the zero state or category view, the delegate's search is stopped - which
  // guarantees that this can't be called.
  SetActivePage(search_results_view_);

  bool clear_stale_results = clear_results_timer_.IsRunning();
  if (clear_stale_results) {
    clear_results_timer_.Stop();
    search_results_view_->ClearSearchResults();
  }

  if (results.empty()) {
    bool no_results_found_shown = search_results_view_->SearchStopped(
        /*illustration=*/{},
        l10n_util::GetStringUTF16(IDS_PICKER_NO_RESULTS_TEXT));
    if (no_results_found_shown) {
      performance_metrics_.MarkSearchResultsUpdated(
          PickerPerformanceMetrics::SearchResultsUpdate::kNoResultsFound);
    } else {
      CHECK(!clear_stale_results)
          << "Stale results were cleared when no results were found, but the "
             "\"no results found\" screen was not shown";
      // `clear_stale_results` must be false here, so nothing happened.
    }
    return;
  }

  for (PickerSearchResultsSection& result : results) {
    search_results_view_->AppendSearchResults(std::move(result));
  }

  PickerPerformanceMetrics::SearchResultsUpdate update;
  if (clear_stale_results) {
    update = PickerPerformanceMetrics::SearchResultsUpdate::kReplace;
  } else {
    update = PickerPerformanceMetrics::SearchResultsUpdate::kAppend;
  }
  performance_metrics_.MarkSearchResultsUpdated(update);
}

void PickerView::SelectCategory(PickerCategory category) {
  SelectCategoryWithQuery(category, /*query=*/u"");
}

void PickerView::SelectCategoryWithQuery(PickerCategory category,
                                         std::u16string_view query) {
  PickerSessionMetrics& session_metrics = delegate_->GetSessionMetrics();
  session_metrics.SetSelectedCategory(category);
  selected_category_ = category;

  if (category == PickerCategory::kEmojisGifs ||
      category == PickerCategory::kEmojis) {
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

  if (category == PickerCategory::kLobster) {
    if (auto* widget = GetWidget()) {
      widget->CloseWithReason(views::Widget::ClosedReason::kLostFocus);
    }
    session_metrics.SetOutcome(
        PickerSessionMetrics::SessionOutcome::kRedirected);
    delegate_->ShowLobster(/*query=*/std::nullopt);
    return;
  }

  search_field_view_->SetPlaceholderText(
      GetSearchFieldPlaceholderTextForPickerCategory(category));
  search_field_view_->SetBackButtonVisible(true);
  SetEmojiBarVisibleIfEnabled(false);
  UpdateSearchQueryAndActivePage(std::u16string(query));
}

void PickerView::PublishCategoryResults(
    PickerCategory category,
    std::vector<PickerSearchResultsSection> results) {
  category_results_view_->ClearSearchResults();

  for (PickerSearchResultsSection& section : results) {
    if (!section.results().empty()) {
      category_results_view_->AppendSearchResults(std::move(section));
    }
  }

  category_results_view_->SearchStopped(GetNoResultsFoundIllustration(),
                                        GetNoResultsFoundDescription(category));
}

void PickerView::AddMainContainerView(PickerLayoutType layout_type) {
  main_container_view_ =
      AddChildView(std::make_unique<PickerMainContainerView>());

  // `base::Unretained` is safe here because this class owns
  // `main_container_view_`, which owns `search_field_view_`.
  search_field_view_ = main_container_view_->AddSearchFieldView(
      views::Builder<PickerSearchFieldView>(
          std::make_unique<PickerSearchFieldView>(
              base::IgnoreArgs<const std::u16string&>(base::BindRepeating(
                  &PickerView::UpdateActivePage, base::Unretained(this))),
              base::BindRepeating(&PickerView::OnSearchBackButtonPressed,
                                  base::Unretained(this)),
              &key_event_handler_, &performance_metrics_))
          .SetPlaceholderText(GetSearchFieldPlaceholderText(
              delegate_->GetMode(),
              IsEditorAvailable(delegate_->GetAvailableCategories())))
          .Build());
  main_container_view_->AddContentsView(layout_type);

  zero_state_view_ =
      main_container_view_->AddPage(std::make_unique<PickerZeroStateView>(
          this, delegate_->GetAvailableCategories(), kPickerViewWidth,
          delegate_->GetAssetFetcher(), &submenu_controller_,
          &preview_controller_));
  category_results_view_ =
      main_container_view_->AddPage(std::make_unique<PickerSearchResultsView>(
          this, kPickerViewWidth, delegate_->GetAssetFetcher(),
          &submenu_controller_, &preview_controller_));
  if (base::FeatureList::IsEnabled(ash::features::kPickerGrid)) {
    category_results_view_->SetLocalFileResultStyle(
        PickerSearchResultsView::LocalFileResultStyle::kGrid);
  }
  search_results_view_ =
      main_container_view_->AddPage(std::make_unique<PickerSearchResultsView>(
          this, kPickerViewWidth, delegate_->GetAssetFetcher(),
          &submenu_controller_, &preview_controller_));

  SetActivePage(zero_state_view_);
}

void PickerView::AddEmojiBarView() {
  emoji_bar_view_ =
      AddChildViewAt(std::make_unique<PickerEmojiBarView>(
                         this, kPickerViewWidth,
                         /*is_gifs_enabled*/ delegate_->IsGifsEnabled()),
                     0);
}

void PickerView::SetActivePage(PickerPageView* page_view) {
  main_container_view_->SetActivePage(page_view);
  SetPseudoFocusedView(nullptr);
  active_item_container_ = page_view;
  SetPseudoFocusedView(active_item_container_->GetTopItem());
  SetWidgetBoundsNeedsUpdate();
}

void PickerView::SetEmojiBarVisibleIfEnabled(bool visible) {
  if (emoji_bar_view_ == nullptr) {
    return;
  }
  emoji_bar_view_->SetVisible(visible);
  SetWidgetBoundsNeedsUpdate();
}

void PickerView::AdvanceActiveItemContainer(
    PickerPseudoFocusDirection direction) {
  if (active_item_container_ == submenu_controller_.GetSubmenuView()) {
    // Just keep the submenu as the active item container.
  } else if (emoji_bar_view_ == nullptr ||
             active_item_container_ == emoji_bar_view_) {
    active_item_container_ = main_container_view_;
  } else {
    active_item_container_ = emoji_bar_view_;
  }
  SetPseudoFocusedView(direction == PickerPseudoFocusDirection::kForward
                           ? active_item_container_->GetTopItem()
                           : active_item_container_->GetBottomItem());
}

void PickerView::SetPseudoFocusedView(views::View* view) {
  if (view == nullptr) {
    SetPseudoFocusedView(search_field_view_->textfield());
    return;
  }

  if (pseudo_focused_view_tracker_.view() == view) {
    return;
  }

  if (IsContainedInSubmenu(view)) {
    active_item_container_ = submenu_controller_.GetSubmenuView();
  } else {
    submenu_controller_.Close();
    if (emoji_bar_view_ != nullptr && emoji_bar_view_->Contains(view)) {
      active_item_container_ = emoji_bar_view_;
    } else {
      active_item_container_ = main_container_view_;
    }
  }

  RemovePickerPseudoFocusFromView(pseudo_focused_view_tracker_.view());

  pseudo_focused_view_tracker_.SetView(view);
  // base::Unretained() is safe here because this class owns
  // `pseudo_focused_view_tracker_`.
  pseudo_focused_view_tracker_.SetIsDeletingCallback(base::BindOnce(
      &PickerView::SetPseudoFocusedView, base::Unretained(this), nullptr));

  search_field_view_->SetTextfieldActiveDescendant(view);
  view->ScrollViewToVisible();
  ApplyPickerPseudoFocusToView(view);
}

views::View* PickerView::GetPseudoFocusedView() {
  return pseudo_focused_view_tracker_.view();
}

void PickerView::OnSearchBackButtonPressed() {
  search_field_view_->SetPlaceholderText(GetSearchFieldPlaceholderText(
      delegate_->GetMode(),
      IsEditorAvailable(delegate_->GetAvailableCategories())));
  search_field_view_->SetBackButtonVisible(false);
  SetEmojiBarVisibleIfEnabled(true);
  selected_category_ = std::nullopt;
  UpdateSearchQueryAndActivePage(u"");
  CHECK_EQ(main_container_view_->active_page(), zero_state_view_)
      << "UpdateSearchQueryAndActivePage did not set active page to zero state "
         "view";
}

void PickerView::ResetEmojiBarToZeroState() {
  if (emoji_bar_view_ == nullptr) {
    return;
  }
  emoji_bar_view_->SetSearchResults(delegate_->GetSuggestedEmoji());
}

bool PickerView::IsContainedInSubmenu(views::View* view) {
  return submenu_controller_.GetSubmenuView() != nullptr &&
         submenu_controller_.GetSubmenuView()->Contains(view);
}

void PickerView::SetWidgetBoundsNeedsUpdate() {
  widget_bounds_needs_update_ = true;
}

BEGIN_METADATA(PickerView)
END_METADATA

}  // namespace ash
