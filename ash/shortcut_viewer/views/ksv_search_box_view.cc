// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shortcut_viewer/views/ksv_search_box_view.h"

#include "ash/constants/ash_features.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/search_box/search_box_constants.h"
#include "ash/search_box/search_box_view_base.h"
#include "ash/shortcut_viewer/strings/grit/shortcut_viewer_strings.h"
#include "ash/style/dark_light_mode_controller_impl.h"
#include "base/metrics/user_metrics.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/border.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/widget/widget.h"

namespace keyboard_shortcut_viewer {

namespace {

constexpr int kIconSize = 20;

// Border corner radius of the search box.
constexpr int kBorderCornerRadius = 32;
constexpr int kBorderThichness = 2;

}  // namespace

KSVSearchBoxView::KSVSearchBoxView(QueryHandler query_handler)
    : query_handler_(std::move(query_handler)) {
  SetSearchBoxBackgroundCornerRadius(kBorderCornerRadius);
  search_box()->SetBackgroundColor(SK_ColorTRANSPARENT);
  search_box()->SetColor(GetPrimaryTextColor());
  SetPlaceholderTextAttributes();
  const std::u16string search_box_name(
      l10n_util::GetStringUTF16(IDS_KSV_SEARCH_BOX_ACCESSIBILITY_NAME));
  search_box()->SetPlaceholderText(search_box_name);
  search_box()->SetAccessibleName(search_box_name);
  SetSearchIconImage(
      gfx::CreateVectorIcon(ash::kKsvSearchBarIcon, GetPrimaryIconColor()));

  views::ImageButton* close_button = CreateCloseButton(base::BindRepeating(
      &KSVSearchBoxView::CloseButtonPressed, base::Unretained(this)));
  close_button->SetHasInkDropActionOnClick(true);
  close_button->SetPreferredSize(gfx::Size(kIconSize, kIconSize));
  close_button->SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  close_button->SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  const std::u16string close_button_label(
      l10n_util::GetStringUTF16(IDS_KSV_CLEAR_SEARCHBOX_ACCESSIBILITY_NAME));
  close_button->SetAccessibleName(close_button_label);
  close_button->SetTooltipText(close_button_label);
}

KSVSearchBoxView::~KSVSearchBoxView() = default;

void KSVSearchBoxView::Initialize() {
  ash::SearchBoxViewBase::InitParams params;
  params.show_close_button_when_active = false;
  params.create_background = true;
  Init(params);
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

void KSVSearchBoxView::OnThemeChanged() {
  ash::SearchBoxViewBase::OnThemeChanged();

  close_button()->SetImageModel(
      views::ImageButton::STATE_NORMAL,
      ui::ImageModel::FromVectorIcon(ash::kKsvSearchCloseIcon,
                                     GetCloseButtonColor()));
  search_box()->SetBackgroundColor(SK_ColorTRANSPARENT);
  search_box()->SetColor(GetPrimaryTextColor());
  search_box()->set_placeholder_text_color(GetPlaceholderTextColor());
  SetBorder(views::CreateRoundedRectBorder(
      kBorderThichness, kBorderCornerRadius, GetBorderColor()));
  SetSearchIconImage(
      gfx::CreateVectorIcon(ash::kKsvSearchBarIcon, GetPrimaryIconColor()));
  UpdateBackgroundColor(GetBackgroundColor());
}

void KSVSearchBoxView::SetAccessibleValue(const std::u16string& value) {
  accessible_value_ = value;
  NotifyAccessibilityEvent(ax::mojom::Event::kValueChanged, true);
}

void KSVSearchBoxView::HandleQueryChange(const std::u16string& query,
                                         bool initiated_by_user) {
  query_handler_.Run(query);
}

void KSVSearchBoxView::UpdateSearchBoxBorder() {
  // TODO(wutao): Rename this function or create another function in base class.
  // It updates many things in addition to the border.
  if (!search_box()->HasFocus() && search_box()->GetText().empty())
    SetSearchBoxActive(false, ui::ET_UNKNOWN);

  if (ShouldUseFocusedColors()) {
    SetBorder(views::CreateRoundedRectBorder(
        kBorderThichness, kBorderCornerRadius, GetBorderColor()));
    return;
  }
  SetBorder(views::CreateRoundedRectBorder(
      kBorderThichness, kBorderCornerRadius, GetBorderColor()));
}

void KSVSearchBoxView::OnSearchBoxActiveChanged(bool active) {
  if (active) {
    base::RecordAction(
        base::UserMetricsAction("KeyboardShortcutViewer.Search"));
  }
}

void KSVSearchBoxView::UpdatePlaceholderTextStyle() {
  SetPlaceholderTextAttributes();
}

void KSVSearchBoxView::SetPlaceholderTextAttributes() {
  search_box()->set_placeholder_text_color(GetPlaceholderTextColor());
  search_box()->set_placeholder_text_draw_flags(
      base::i18n::IsRTL() ? gfx::Canvas::TEXT_ALIGN_RIGHT
                          : gfx::Canvas::TEXT_ALIGN_LEFT);
}

void KSVSearchBoxView::CloseButtonPressed() {
  // After clicking search box close button focus the search box text field.
  search_box()->RequestFocus();
  ClearSearch();
}

SkColor KSVSearchBoxView::GetBackgroundColor() {
  return GetColorProvider()->GetColor(cros_tokens::kToolbarSearchBgColor);
}

SkColor KSVSearchBoxView::GetBorderColor() {
  if (!ShouldUseFocusedColors()) {
    return SK_ColorTRANSPARENT;
  }

  return GetColorProvider()->GetColor(ui::kColorAshFocusRing);
}

SkColor KSVSearchBoxView::GetCloseButtonColor() {
  return ShouldUseDarkThemeColors() ? gfx::kGoogleGrey400 : gfx::kGoogleGrey700;
}

SkColor KSVSearchBoxView::GetPlaceholderTextColor() {
  return ShouldUseDarkThemeColors() ? gfx::kGoogleGrey400
                                    : ash::kZeroQuerySearchboxColor;
}

SkColor KSVSearchBoxView::GetPrimaryIconColor() {
  return ShouldUseDarkThemeColors() ? gfx::kGoogleGrey200 : gfx::kGoogleGrey900;
}

SkColor KSVSearchBoxView::GetPrimaryTextColor() {
  return ShouldUseDarkThemeColors() ? gfx::kGoogleGrey200 : gfx::kGoogleGrey900;
}

bool KSVSearchBoxView::ShouldUseFocusedColors() {
  return search_box()->HasFocus() || is_search_box_active();
}

bool KSVSearchBoxView::ShouldUseDarkThemeColors() {
  return ash::DarkLightModeControllerImpl::Get()->IsDarkModeEnabled();
}

BEGIN_METADATA(KSVSearchBoxView)
END_METADATA

}  // namespace keyboard_shortcut_viewer
