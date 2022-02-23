// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shortcut_viewer/views/ksv_search_box_view.h"

#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/search_box/search_box_view_delegate.h"
#include "ash/shortcut_viewer/strings/grit/shortcut_viewer_strings.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/textfield/textfield.h"

namespace keyboard_shortcut_viewer {

namespace {

constexpr int kIconSize = 20;

// Border corner radius of the search box.
constexpr int kBorderCornerRadius = 32;

}  // namespace

KSVSearchBoxView::KSVSearchBoxView(ash::SearchBoxViewDelegate* delegate)
    : ash::SearchBoxViewBase(delegate) {
  SetSearchBoxBackgroundCornerRadius(kBorderCornerRadius);
  UpdateBackgroundColor(
      ash::AppListColorProvider::Get()->GetSearchBoxBackgroundColor());
  search_box()->SetBackgroundColor(SK_ColorTRANSPARENT);
  search_box()->SetColor(
      ash::AppListColorProvider::Get()->GetSearchBoxTextColor(
          gfx::kGoogleGrey900));
  SetPlaceholderTextAttributes();
  const std::u16string search_box_name(
      l10n_util::GetStringUTF16(IDS_KSV_SEARCH_BOX_ACCESSIBILITY_NAME));
  search_box()->SetPlaceholderText(search_box_name);
  search_box()->SetAccessibleName(search_box_name);
  SetSearchIconImage(gfx::CreateVectorIcon(
      ash::kKsvSearchBarIcon,
      ash::AppListColorProvider::Get()->GetSearchBoxIconColor(
          gfx::kGoogleGrey900)));
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

void KSVSearchBoxView::SetAccessibleValue(const std::u16string& value) {
  accessible_value_ = value;
  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
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
  UpdateBackgroundColor(
      ash::AppListColorProvider::Get()->GetSearchBoxBackgroundColor());
}

void KSVSearchBoxView::SetupCloseButton() {
  views::ImageButton* close = close_button();
  close->SetHasInkDropActionOnClick(true);
  close->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(ash::kKsvSearchCloseIcon, gfx::kGoogleGrey700));
  close->SetPreferredSize(gfx::Size(kIconSize, kIconSize));
  close->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  close->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  const std::u16string close_button_label(
      l10n_util::GetStringUTF16(IDS_KSV_CLEAR_SEARCHBOX_ACCESSIBILITY_NAME));
  close->SetAccessibleName(close_button_label);
  close->SetTooltipText(close_button_label);
  close->SetVisible(false);
}

void KSVSearchBoxView::SetupBackButton() {
  views::ImageButton* back = back_button();
  back->SetHasInkDropActionOnClick(true);
  back->SetImage(
      views::ImageButton::STATE_NORMAL,
      gfx::CreateVectorIcon(ash::kKsvSearchBackIcon, gfx::kGoogleBlue500));
  back->SetPreferredSize(gfx::Size(kIconSize, kIconSize));
  back->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  back->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  const std::u16string back_button_label(
      l10n_util::GetStringUTF16(IDS_KSV_BACK_ACCESSIBILITY_NAME));
  back->SetAccessibleName(back_button_label);
  back->SetTooltipText(back_button_label);
  back->SetVisible(false);
}

void KSVSearchBoxView::UpdatePlaceholderTextStyle() {
  SetPlaceholderTextAttributes();
}

void KSVSearchBoxView::SetPlaceholderTextAttributes() {
  search_box()->set_placeholder_text_color(
      ash::AppListColorProvider::Get()->GetSearchBoxSecondaryTextColor(
          ash::kZeroQuerySearchboxColor));
  search_box()->set_placeholder_text_draw_flags(
      base::i18n::IsRTL() ? gfx::Canvas::TEXT_ALIGN_RIGHT
                          : gfx::Canvas::TEXT_ALIGN_LEFT);
}

}  // namespace keyboard_shortcut_viewer
