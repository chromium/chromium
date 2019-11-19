// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/shortcut_viewer/views/ksv_search_box_view.h"

#include "ash/components/shortcut_viewer/vector_icons/vector_icons.h"
#include "ash/components/strings/grit/ash_components_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/search_box/search_box_view_delegate.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/textfield/textfield.h"

namespace keyboard_shortcut_viewer {

namespace {

constexpr SkColor kDefaultSearchBoxBackgroundColor =
    SkColorSetARGB(0x28, 0x5F, 0x63, 0x68);

constexpr int kIconSize = 20;

// Border corner radius of the search box.
constexpr int kBorderCornerRadius = 32;

}  // namespace

KSVSearchBoxView::KSVSearchBoxView(search_box::SearchBoxViewDelegate* delegate)
    : search_box::SearchBoxViewBase(delegate) {
  SetSearchBoxBackgroundCornerRadius(kBorderCornerRadius);
  UpdateBackgroundColor(kDefaultSearchBoxBackgroundColor);
  search_box()->SetBackgroundColor(SK_ColorTRANSPARENT);
  search_box()->SetColor(gfx::kGoogleGrey900);
  search_box()->set_placeholder_text_color(gfx::kGoogleGrey900);
  search_box()->set_placeholder_text_draw_flags(gfx::Canvas::TEXT_ALIGN_CENTER);
  const base::string16 search_box_name(
      l10n_util::GetStringUTF16(IDS_KSV_SEARCH_BOX_ACCESSIBILITY_NAME));
  search_box()->SetPlaceholderText(search_box_name);
  search_box()->SetAccessibleName(search_box_name);
  SetSearchIconImage(
      gfx::CreateVectorIcon(kKsvSearchBarIcon, gfx::kGoogleGrey900));
}

gfx::Size KSVSearchBoxView::CalculatePreferredSize() const {
  return gfx::Size(740, 32);
}

void KSVSearchBoxView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kSearchBox;
  node_data->SetValue(accessible_value_);
}

void KSVSearchBoxView::OnKeyEvent(ui::KeyEvent* event) {
  const ui::KeyboardCode key = event->key_code();
  if ((key != ui::VKEY_ESCAPE && key != ui::VKEY_BROWSER_BACK) ||
      event->type() != ui::ET_KEY_PRESSED) {
    return;
  }

  event->SetHandled();
  // |VKEY_BROWSER_BACK| will only clear all the text.
  ClearSearch();
  // |VKEY_ESCAPE| will clear text and exit search mode directly.
  if (key == ui::VKEY_ESCAPE)
    SetSearchBoxActive(false, event->type());
}

void KSVSearchBoxView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  // Focus on the search box text field after clicking close button.
  if (close_button() && sender == close_button())
    search_box()->RequestFocus();
  SearchBoxViewBase::ButtonPressed(sender, event);
}

void KSVSearchBoxView::SetAccessibleValue(const base::string16& value) {
  accessible_value_ = value;
  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
}

void KSVSearchBoxView::UpdateBackgroundColor(SkColor color) {
  GetSearchBoxBackground()->SetNativeControlColor(color);
}

void KSVSearchBoxView::UpdateSearchBoxBorder() {
  // TODO(wutao): Rename this function or create another function in base class.
  // It updates many things in addition to the border.
  if (!search_box()->HasFocus() && search_box()->GetText().empty())
    SetSearchBoxActive(false, ui::ET_UNKNOWN);

  constexpr int kBorderThichness = 2;
  constexpr SkColor kActiveBorderColor = SkColorSetARGB(0x7F, 0x1A, 0x73, 0xE8);

  if (search_box()->HasFocus() || is_search_box_active()) {
    SetBorder(views::CreateRoundedRectBorder(
        kBorderThichness, kBorderCornerRadius, kActiveBorderColor));
    UpdateBackgroundColor(gfx::kGoogleGrey100);
    return;
  }
  SetBorder(views::CreateRoundedRectBorder(
      kBorderThichness, kBorderCornerRadius, SK_ColorTRANSPARENT));
  UpdateBackgroundColor(kDefaultSearchBoxBackgroundColor);
}

void KSVSearchBoxView::SetupCloseButton() {
  views::ImageButton* close = close_button();
  close->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(kKsvSearchCloseIcon, gfx::kGoogleGrey700));
  close->SetPreferredSize(gfx::Size(kIconSize, kIconSize));
  close->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  close->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  const base::string16 close_button_label(
      l10n_util::GetStringUTF16(IDS_KSV_CLEAR_SEARCHBOX_ACCESSIBILITY_NAME));
  close->SetAccessibleName(close_button_label);
  close->SetTooltipText(close_button_label);
  close->SetVisible(false);
}

void KSVSearchBoxView::SetupBackButton() {
  views::ImageButton* back = back_button();
  back->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(kKsvSearchBackIcon, gfx::kGoogleBlue500));
  back->SetPreferredSize(gfx::Size(kIconSize, kIconSize));
  back->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  back->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  const base::string16 back_button_label(
      l10n_util::GetStringUTF16(IDS_KSV_BACK_ACCESSIBILITY_NAME));
  back->SetAccessibleName(back_button_label);
  back->SetTooltipText(back_button_label);
  back->SetVisible(false);
}

}  // namespace keyboard_shortcut_viewer
