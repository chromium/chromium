// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_suggestion_chip_view.h"

#include <algorithm>
#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr SkColor kRippleColor = SkColorSetA(gfx::kGoogleGrey100, 0x0F);
constexpr SkColor kFocusRingColor = gfx::kGoogleBlue300;
constexpr int kMaxTextWidth = 192;
constexpr int kBlurRadius = 5;
constexpr int kIconMarginDip = 8;
constexpr int kPaddingDip = 16;
constexpr int kPreferredHeightDip = 32;

// Records an app being launched.
void LogAppLaunch(int index_in_container) {
  DCHECK_GE(index_in_container, 0);
  base::UmaHistogramSparse("Apps.AppListSuggestedChipLaunched",
                           index_in_container);

  base::RecordAction(base::UserMetricsAction("AppList_OpenSuggestedApp"));
}

}  // namespace

SearchResultSuggestionChipView::SearchResultSuggestionChipView(
    AppListViewDelegate* view_delegate)
    : view_delegate_(view_delegate),
      icon_view_(new views::ImageView()),
      text_view_(new views::Label()) {
  SetFocusBehavior(FocusBehavior::ALWAYS);

  SetInstallFocusRingOnFocus(true);
  focus_ring()->SetColor(kFocusRingColor);

  SetInkDropMode(InkDropMode::ON);
  views::InstallPillHighlightPathGenerator(this);

  InitLayout();
}

SearchResultSuggestionChipView::~SearchResultSuggestionChipView() = default;

void SearchResultSuggestionChipView::SetBackgroundBlurEnabled(bool enabled) {
  // Background blur is enabled if and only if layer exists.
  if (!!layer() == enabled)
    return;

  if (!enabled) {
    DestroyLayer();
    return;
  }

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetBackgroundBlur(kBlurRadius);
  layer()->SetName("launcher/SearchResultSuggestionChip");
  SetRoundedCornersForLayer(kPreferredHeightDip / 2);
}

void SearchResultSuggestionChipView::OnResultChanged() {
  SetVisible(!!result());
  UpdateSuggestionChipView();
}

void SearchResultSuggestionChipView::OnMetadataChanged() {
  UpdateSuggestionChipView();
}

void SearchResultSuggestionChipView::ButtonPressed(views::Button* sender,
                                                   const ui::Event& event) {
  DCHECK(result());
  LogAppLaunch(index_in_container());
  RecordSearchResultOpenSource(result(), view_delegate_->GetModel(),
                               view_delegate_->GetSearchModel());
  view_delegate_->OpenSearchResult(
      result()->id(), event.flags(),
      AppListLaunchedFrom::kLaunchedFromSuggestionChip,
      AppListLaunchType::kAppSearchResult, index_in_container(),
      false /* launch_as_default */);
}

const char* SearchResultSuggestionChipView::GetClassName() const {
  return "SearchResultSuggestionChipView";
}

void SearchResultSuggestionChipView::ChildVisibilityChanged(
    views::View* child) {
  // When icon visibility is modified we need to update layout padding.
  if (child == icon_view_) {
    const int padding_left_dip =
        icon_view_->GetVisible() ? kIconMarginDip : kPaddingDip;
    layout_manager_->set_inside_border_insets(
        gfx::Insets(0, padding_left_dip, 0, kPaddingDip));
  }
  PreferredSizeChanged();
}

void SearchResultSuggestionChipView::OnPaintBackground(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);

  gfx::Rect bounds = GetContentsBounds();

  // Background.
  flags.setColor(
      AppListColorProvider::Get()->GetSuggestionChipBackgroundColor());
  canvas->DrawRoundRect(bounds, height() / 2, flags);

  // Focus Ring should only be visible when keyboard traversal is occurring.
  if (view_delegate_->KeyboardTraversalEngaged())
    focus_ring()->SetColor(kFocusRingColor);
  else
    focus_ring()->SetColor(SkColorSetA(kFocusRingColor, 0));
}

void SearchResultSuggestionChipView::OnFocus() {
  SchedulePaint();
  NotifyAccessibilityEvent(ax::mojom::Event::kFocus, true);
}

void SearchResultSuggestionChipView::OnBlur() {
  SchedulePaint();
}

bool SearchResultSuggestionChipView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE)
    return false;
  return Button::OnKeyPressed(event);
}

std::unique_ptr<views::InkDrop>
SearchResultSuggestionChipView::CreateInkDrop() {
  std::unique_ptr<views::InkDropImpl> ink_drop =
      Button::CreateDefaultInkDropImpl();
  ink_drop->SetShowHighlightOnHover(false);
  ink_drop->SetShowHighlightOnFocus(false);
  ink_drop->SetAutoHighlightMode(views::InkDropImpl::AutoHighlightMode::NONE);
  return std::move(ink_drop);
}

std::unique_ptr<views::InkDropRipple>
SearchResultSuggestionChipView::CreateInkDropRipple() const {
  const gfx::Point center = GetLocalBounds().CenterPoint();
  const int ripple_radius = width() / 2;
  gfx::Rect bounds(center.x() - ripple_radius, center.y() - ripple_radius,
                   2 * ripple_radius, 2 * ripple_radius);
  return std::make_unique<views::FloodFillInkDropRipple>(
      size(), GetLocalBounds().InsetsFrom(bounds),
      GetInkDropCenterBasedOnLastEvent(), kRippleColor, 1.0f);
}

std::unique_ptr<ui::Layer> SearchResultSuggestionChipView::RecreateLayer() {
  std::unique_ptr<ui::Layer> old_layer = views::View::RecreateLayer();
  if (layer())
    SetRoundedCornersForLayer(kPreferredHeightDip / 2);
  return old_layer;
}

void SearchResultSuggestionChipView::SetIcon(const gfx::ImageSkia& icon) {
  icon_view_->SetImage(icon);
  icon_view_->SetVisible(true);
}

void SearchResultSuggestionChipView::SetText(const base::string16& text) {
  text_view_->SetText(text);
  gfx::Size size = text_view_->CalculatePreferredSize();
  size.set_width(std::min(kMaxTextWidth, size.width()));
  text_view_->SetPreferredSize(size);
}

const base::string16& SearchResultSuggestionChipView::GetText() const {
  return text_view_->GetText();
}

void SearchResultSuggestionChipView::UpdateSuggestionChipView() {
  if (!result()) {
    SetIcon(gfx::ImageSkia());
    SetText(base::string16());
    SetAccessibleName(base::string16());
    return;
  }

  SetIcon(result()->chip_icon());
  SetText(result()->title());

  base::string16 accessible_name = result()->title();
  if (result()->id() == kInternalAppIdContinueReading) {
    accessible_name = l10n_util::GetStringFUTF16(
        IDS_APP_LIST_CONTINUE_READING_ACCESSIBILE_NAME, accessible_name);
  }
  SetAccessibleName(accessible_name);
}

void SearchResultSuggestionChipView::InitLayout() {
  layout_manager_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(0, kPaddingDip, 0, kPaddingDip), kIconMarginDip));

  layout_manager_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Icon.
  const int icon_size =
      AppListConfig::instance().suggestion_chip_icon_dimension();
  icon_view_->SetImageSize(gfx::Size(icon_size, icon_size));
  icon_view_->SetPreferredSize(gfx::Size(icon_size, icon_size));

  icon_view_->SetVisible(false);
  AddChildView(icon_view_);

  // Text.
  text_view_->SetAutoColorReadabilityEnabled(false);
  text_view_->SetSubpixelRenderingEnabled(false);
  text_view_->SetFontList(AppListConfig::instance().app_title_font());
  SetText(base::string16());
  text_view_->SetEnabledColor(
      AppListColorProvider::Get()->GetSuggestionChipTextColor());
  AddChildView(text_view_);
}

void SearchResultSuggestionChipView::SetRoundedCornersForLayer(
    int corner_radius) {
  layer()->SetRoundedCornerRadius(
      {corner_radius, corner_radius, corner_radius, corner_radius});
  layer()->SetIsFastRoundedCorner(true);
}

}  // namespace ash
