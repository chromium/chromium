// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_field_view.h"

#include <string>
#include <string_view>

#include "ash/ash_element_identifiers.h"
#include "ash/picker/metrics/picker_performance_metrics.h"
#include "ash/picker/views/picker_focus_indicator.h"
#include "ash/picker/views/picker_key_event_handler.h"
#include "ash/picker/views/picker_pseudo_focus.h"
#include "ash/picker/views/picker_search_bar_textfield.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/icon_button.h"
#include "ash/style/style_util.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/time/time.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/range/range.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr auto kSearchFieldVerticalPadding = gfx::Insets::VH(6, 0);
constexpr auto kButtonHorizontalMargin = gfx::Insets::VH(0, 8);
// The default horizontal margin for the textfield when surrounding icon buttons
// are not visible.
constexpr int kDefaultTextfieldHorizontalMargin = 16;
// Margins around the textfield focus indicator bar.
constexpr auto kTextfieldFocusIndicatorMargins = gfx::Insets::VH(6, 0);

}  // namespace

PickerSearchFieldView::PickerSearchFieldView(
    SearchCallback search_callback,
    BackCallback back_callback,
    PickerKeyEventHandler* key_event_handler,
    PickerPerformanceMetrics* performance_metrics)
    : search_callback_(std::move(search_callback)),
      key_event_handler_(key_event_handler),
      performance_metrics_(performance_metrics) {
  views::Builder<PickerSearchFieldView>(this)
      .SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetCrossAxisAlignment(views::LayoutAlignment::kCenter)
      .SetProperty(views::kMarginsKey, kSearchFieldVerticalPadding)
      .AddChildren(
          views::Builder<views::ImageButton>(
              std::make_unique<IconButton>(
                  std::move(back_callback), IconButton::Type::kSmallFloating,
                  &vector_icons::kArrowBackIcon, IDS_ACCNAME_BACK))
              .CopyAddressTo(&back_button_)
              .SetProperty(views::kMarginsKey, kButtonHorizontalMargin)
              .SetVisible(false),
          views::Builder<PickerSearchBarTextfield>(
              std::make_unique<PickerSearchBarTextfield>(this))
              .CopyAddressTo(&textfield_)
              .SetProperty(views::kElementIdentifierKey,
                           kPickerSearchFieldTextfieldElementId)
              .SetController(this)
              .SetBackgroundColor(SK_ColorTRANSPARENT)
              .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
                  TypographyToken::kCrosBody2))
              .SetProperty(views::kBoxLayoutFlexKey,
                           views::BoxLayoutFlexSpecification().WithWeight(1)))
      .AddChild(views::Builder<views::ImageButton>(
                    std::make_unique<IconButton>(
                        // `base::Unretained` is safe here since the search
                        // field is owned by this class.
                        base::BindRepeating(
                            &PickerSearchFieldView::ClearButtonPressed,
                            base::Unretained(this)),
                        IconButton::Type::kSmallFloating, &views::kIcCloseIcon,
                        IDS_APP_LIST_CLEAR_SEARCHBOX))
                    .CopyAddressTo(&clear_button_)
                    .SetProperty(views::kMarginsKey, kButtonHorizontalMargin)
                    .SetVisible(false))
      .BuildChildren();

  StyleUtil::SetUpInkDropForButton(back_button_, gfx::Insets(),
                                   /*highlight_on_hover=*/true,
                                   /*highlight_on_focus=*/true);
  StyleUtil::SetUpInkDropForButton(clear_button_, gfx::Insets(),
                                   /*highlight_on_hover=*/true,
                                   /*highlight_on_focus=*/true);

  UpdateTextfieldBorder();
}

PickerSearchFieldView::~PickerSearchFieldView() = default;

void PickerSearchFieldView::RequestFocus() {
  textfield_->RequestFocus();
}

void PickerSearchFieldView::AddedToWidget() {
  GetFocusManager()->AddFocusChangeListener(this);
}

void PickerSearchFieldView::RemovedFromWidget() {
  GetFocusManager()->RemoveFocusChangeListener(this);
}

void PickerSearchFieldView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  if (should_show_focus_indicator_) {
    PaintPickerFocusIndicator(
        canvas, gfx::Point(0, kTextfieldFocusIndicatorMargins.top()),
        height() - kTextfieldFocusIndicatorMargins.height(),
        GetColorProvider()->GetColor(cros_tokens::kCrosSysFocusRing));
  }
}

void PickerSearchFieldView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  ContentsChangedInternal(new_contents);

  search_callback_.Run(new_contents);
}

void PickerSearchFieldView::ContentsChangedInternal(
    std::u16string_view new_contents) {
  performance_metrics_->MarkContentsChanged();

  // Show the clear button only when the query is not empty.
  clear_button_->SetVisible(!new_contents.empty());
  UpdateTextfieldBorder();

  ScheduleNotifyInitialActiveDescendantForA11y();
}

bool PickerSearchFieldView::HandleKeyEvent(views::Textfield* sender,
                                           const ui::KeyEvent& key_event) {
  return key_event_handler_->HandleKeyEvent(key_event);
}

void PickerSearchFieldView::OnWillChangeFocus(View* focused_before,
                                              View* focused_now) {}

void PickerSearchFieldView::OnDidChangeFocus(View* focused_before,
                                             View* focused_now) {
  if (focused_now == textfield_) {
    performance_metrics_->MarkInputFocus();
  }

  ScheduleNotifyInitialActiveDescendantForA11y();
}

const std::u16string& PickerSearchFieldView::GetPlaceholderText() const {
  return textfield_->GetPlaceholderText();
}

void PickerSearchFieldView::SetPlaceholderText(
    const std::u16string& new_placeholder_text) {
  textfield_->SetPlaceholderText(new_placeholder_text);
  textfield_->GetViewAccessibility().SetName(new_placeholder_text);
}

void PickerSearchFieldView::SetTextfieldActiveDescendant(views::View* view) {
  // If the initial active descendant has not been announced yet, then track
  // this descendant so it can be announced when the timer fires.
  if (!textfield_->HasFocus() ||
      notify_initial_active_descendant_timer_.IsRunning()) {
    active_descendant_tracker_.SetView(view);
    return;
  }

  // The initial active descendant has been announced, so announce this
  // descendant immediately.
  if (view) {
    textfield_->GetViewAccessibility().SetActiveDescendant(*view);
  } else {
    textfield_->GetViewAccessibility().ClearActiveDescendant();
  }

  active_descendant_tracker_.SetView(nullptr);
}

std::u16string_view PickerSearchFieldView::GetQueryText() const {
  return textfield_->GetText();
}

void PickerSearchFieldView::SetQueryText(std::u16string text) {
  if (text != GetQueryText()) {
    textfield_->SetText(std::move(text));
    ContentsChangedInternal(GetQueryText());
  }
}

void PickerSearchFieldView::SetBackButtonVisible(bool visible) {
  back_button_->SetVisible(visible);
  UpdateTextfieldBorder();
}

void PickerSearchFieldView::SetShouldShowFocusIndicator(
    bool should_show_focus_indicator) {
  if (should_show_focus_indicator_ == should_show_focus_indicator) {
    return;
  }
  should_show_focus_indicator_ = should_show_focus_indicator;
  SchedulePaint();
}

views::View* PickerSearchFieldView::GetViewLeftOf(views::View* view) {
  if (!Contains(view)) {
    return nullptr;
  }
  views::View* left_view = GetNextPickerPseudoFocusableView(
      view, PickerPseudoFocusDirection::kBackward, /*should_loop=*/false);
  return Contains(left_view) ? left_view : nullptr;
}

views::View* PickerSearchFieldView::GetViewRightOf(views::View* view) {
  if (!Contains(view)) {
    return nullptr;
  }
  views::View* right_view = GetNextPickerPseudoFocusableView(
      view, PickerPseudoFocusDirection::kForward, /*should_loop=*/false);
  return Contains(right_view) ? right_view : nullptr;
}

bool PickerSearchFieldView::LeftEventShouldMoveCursor(
    views::View* pseudo_focused_view) {
  if (pseudo_focused_view == textfield_ &&
      textfield_->GetCursorPosition() != GetQueryStartIndexForTraversal()) {
    return true;
  }
  return GetViewLeftOf(pseudo_focused_view) == nullptr;
}

bool PickerSearchFieldView::RightEventShouldMoveCursor(
    views::View* pseudo_focused_view) {
  if (pseudo_focused_view == textfield_ &&
      textfield_->GetCursorPosition() != GetQueryEndIndexForTraversal()) {
    return true;
  }
  return GetViewRightOf(pseudo_focused_view) == nullptr;
}

void PickerSearchFieldView::OnGainedPseudoFocusFromLeftEvent(
    views::View* pseudo_focused_view) {
  if (pseudo_focused_view == textfield_) {
    textfield_->SetSelectedRange(gfx::Range(GetQueryEndIndexForTraversal()));
  }
}

void PickerSearchFieldView::OnGainedPseudoFocusFromRightEvent(
    views::View* pseudo_focused_view) {
  if (pseudo_focused_view == textfield_) {
    textfield_->SetSelectedRange(gfx::Range(GetQueryStartIndexForTraversal()));
  }
}

void PickerSearchFieldView::ClearButtonPressed() {
  textfield_->SetText(u"");
  ContentsChanged(textfield_, u"");
}

void PickerSearchFieldView::UpdateTextfieldBorder() {
  textfield_->SetBorder(views::CreateEmptyBorder(gfx::Insets::TLBR(
      0, back_button_->GetVisible() ? 0 : kDefaultTextfieldHorizontalMargin, 0,
      clear_button_->GetVisible() ? 0 : kDefaultTextfieldHorizontalMargin)));
}

void PickerSearchFieldView::ScheduleNotifyInitialActiveDescendantForA11y() {
  // Delay the active descendant change so that:
  // (1) There's no jarring transition of the screen reader's focus rectangle.
  // (2) There's time for the screen reader to read out the change to input
  // field contents.
  notify_initial_active_descendant_timer_.Start(
      FROM_HERE, kNotifyInitialActiveDescendantA11yDelay,
      base::BindOnce(
          &PickerSearchFieldView::NotifyInitialActiveDescendantForA11y,
          base::Unretained(this)));
}

void PickerSearchFieldView::NotifyInitialActiveDescendantForA11y() {
  if (active_descendant_tracker_) {
    SetTextfieldActiveDescendant(active_descendant_tracker_.view());
  }
}

size_t PickerSearchFieldView::GetQueryStartIndexForTraversal() {
  // The query start index should actually be the same regardless of text
  // direction, but we reverse it here since left / right key events are swapped
  // when traversing Picker UI in RTL.
  return base::i18n::IsRTL() ? GetQueryText().length() : 0;
}

size_t PickerSearchFieldView::GetQueryEndIndexForTraversal() {
  // The query end index should actually be the same regardless of text
  // direction, but we reverse it here since left / right key events are swapped
  // when traversing Picker UI in RTL.
  return base::i18n::IsRTL() ? 0 : GetQueryText().length();
}

BEGIN_METADATA(PickerSearchFieldView)
END_METADATA

}  // namespace ash
