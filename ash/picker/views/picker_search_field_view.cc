// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_search_field_view.h"

#include <string>
#include <string_view>

#include "ash/ash_element_identifiers.h"
#include "ash/picker/metrics/picker_performance_metrics.h"
#include "ash/picker/views/picker_key_event_handler.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/vector_icons.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

constexpr auto kSearchFieldVerticalPadding = gfx::Insets::VH(6, 0);
constexpr auto kButtonHorizontalMargin = gfx::Insets::VH(0, 8);
// The default horizontal margin for the textfield when surrounding icon buttons
// are not visible.
constexpr int kDefaultTextfieldHorizontalMargin = 16;

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
              views::ImageButton::CreateIconButton(
                  std::move(back_callback), vector_icons::kArrowBackIcon,
                  // TODO(b/309706053): Replace this once the strings are
                  // finalized.
                  u"Placeholder",
                  views::ImageButton::MaterialIconStyle::kSmall))
              .CopyAddressTo(&back_button_)
              .SetProperty(views::kMarginsKey, kButtonHorizontalMargin)
              .SetVisible(false),
          views::Builder<views::Textfield>()
              .CopyAddressTo(&textfield_)
              .SetProperty(views::kElementIdentifierKey,
                           kPickerSearchFieldTextfieldElementId)
              .SetController(this)
              .SetBackgroundColor(SK_ColorTRANSPARENT)
              .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
                  TypographyToken::kCrosBody2))
              .SetProperty(views::kFlexBehaviorKey,
                           views::FlexSpecification(
                               views::LayoutOrientation::kHorizontal,
                               views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded))
              // TODO(b/309706053): Replace this once the strings are finalized.
              .SetAccessibleName(u"placeholder"))
      .AddChild(
          views::Builder<views::ImageButton>(
              views::ImageButton::CreateIconButton(
                  // `base::Unretained` is safe here since the search field is
                  // owned by this class.
                  base::BindRepeating(
                      &PickerSearchFieldView::ClearButtonPressed,
                      base::Unretained(this)),
                  views::kIcCloseIcon, u"placeholder",
                  views::ImageButton::MaterialIconStyle::kSmall))
              .CopyAddressTo(&clear_button_)
              .SetProperty(views::kMarginsKey, kButtonHorizontalMargin)
              .SetVisible(false)
              // TODO(b/309706053): Replace this once the strings are finalized.
              .SetAccessibleName(u"placeholder"))
      .BuildChildren();

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

void PickerSearchFieldView::ContentsChanged(
    views::Textfield* sender,
    const std::u16string& new_contents) {
  performance_metrics_->MarkContentsChanged();

  // Show the clear button only when the query is not empty.
  clear_button_->SetVisible(!new_contents.empty());
  UpdateTextfieldBorder();

  search_callback_.Run(new_contents);
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
}

const std::u16string& PickerSearchFieldView::GetPlaceholderText() const {
  return textfield_->GetPlaceholderText();
}

void PickerSearchFieldView::SetPlaceholderText(
    const std::u16string& new_placeholder_text) {
  textfield_->SetPlaceholderText(new_placeholder_text);
}

void PickerSearchFieldView::SetTextfieldActiveDescendant(views::View* view) {
  if (view) {
    textfield_->GetViewAccessibility().SetActiveDescendant(*view);
  } else {
    textfield_->GetViewAccessibility().ClearActiveDescendant();
  }

  textfield_->NotifyAccessibilityEvent(
      ax::mojom::Event::kActiveDescendantChanged, true);
}

std::u16string_view PickerSearchFieldView::GetQueryText() const {
  return textfield_->GetText();
}

void PickerSearchFieldView::SetQueryText(std::u16string text) {
  textfield_->SetText(std::move(text));
}

void PickerSearchFieldView::SetBackButtonVisible(bool visible) {
  back_button_->SetVisible(visible);
  UpdateTextfieldBorder();
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

BEGIN_METADATA(PickerSearchFieldView)
END_METADATA

}  // namespace ash
