// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_suggestion_chip_view.h"

#include <algorithm>
#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/color/color_id.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

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

// Copied from AppListColorProvider.
bool ShouldUseDarkLightColors() {
  return features::IsDarkLightModeEnabled() ||
         features::IsProductivityLauncherEnabled();
}

}  // namespace

SearchResultSuggestionChipView::SearchResultSuggestionChipView(
    AppListViewDelegate* view_delegate)
    : focus_ring_color_(ShouldUseDarkLightColors()
                            ? ui::kColorAshFocusRing
                            : ui::kColorAshAppListFocusRingCompat),
      view_delegate_(view_delegate) {
  SetFocusBehavior(FocusBehavior::ALWAYS);
  // TODO(crbug.com/1218186): Remove this, this is in place temporarily to be
  // able to submit accessibility checks, but this focusable View needs to
  // add a name so that the screen reader knows what to announce.
  SetProperty(views::kSkipAccessibilityPaintChecks, true);
  SetCallback(
      base::BindRepeating(&SearchResultSuggestionChipView::OnButtonPressed,
                          base::Unretained(this)));

  SetInstallFocusRingOnFocus(true);
  views::FocusRing::Get(this)->SetColorId(focus_ring_color_);

  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  views::InstallPillHighlightPathGenerator(this);
  views::InkDrop::UseInkDropWithoutAutoHighlight(views::InkDrop::Get(this),
                                                 /*highlight_on_hover=*/false);
  views::InkDrop::Get(this)->SetCreateRippleCallback(base::BindRepeating(
      [](Button* host) -> std::unique_ptr<views::InkDropRipple> {
        const gfx::Point center = host->GetLocalBounds().CenterPoint();
        const int ripple_radius = host->width() / 2;
        const gfx::Rect bounds(center.x() - ripple_radius,
                               center.y() - ripple_radius, 2 * ripple_radius,
                               2 * ripple_radius);
        const AppListColorProvider* const color_provider =
            AppListColorProvider::Get();
        const SkColor bg_color = color_provider->GetSearchBoxBackgroundColor();
        return std::make_unique<views::FloodFillInkDropRipple>(
            host->size(), host->GetLocalBounds().InsetsFrom(bounds),
            views::InkDrop::Get(host)->GetInkDropCenterBasedOnLastEvent(),
            color_provider->GetInkDropBaseColor(bg_color),
            color_provider->GetInkDropOpacity(bg_color));
      },
      this));

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
        gfx::Insets::TLBR(0, padding_left_dip, 0, kPaddingDip));
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
  views::FocusRing::Get(this)->SetColorId(
      view_delegate_->KeyboardTraversalEngaged()
          ? focus_ring_color_
          : ui::kColorAshAppListFocusRingNoKeyboard);
}

void SearchResultSuggestionChipView::OnFocus() {
  SchedulePaint();
  SearchResultBaseView::OnFocus();
}

void SearchResultSuggestionChipView::OnBlur() {
  SchedulePaint();
}

bool SearchResultSuggestionChipView::OnKeyPressed(const ui::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE)
    return false;
  return Button::OnKeyPressed(event);
}

void SearchResultSuggestionChipView::OnThemeChanged() {
  views::View::OnThemeChanged();
  text_view_->SetEnabledColor(
      AppListColorProvider::Get()->GetSuggestionChipTextColor());
  SchedulePaint();
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

void SearchResultSuggestionChipView::SetText(const std::u16string& text) {
  text_view_->SetText(text);
  gfx::Size size = text_view_->CalculatePreferredSize();
  size.set_width(std::min(kMaxTextWidth, size.width()));
  text_view_->SetPreferredSize(size);
}

const std::u16string& SearchResultSuggestionChipView::GetText() const {
  return text_view_->GetText();
}

void SearchResultSuggestionChipView::UpdateSuggestionChipView() {
  if (!result()) {
    SetIcon(gfx::ImageSkia());
    if (!GetText().empty())
      SetText(std::u16string());
    SetAccessibleName(std::u16string());
    return;
  }

  SetIcon(result()->chip_icon());
  SetText(result()->title());

  std::u16string accessible_name = result()->title();
  if (result()->id() == kInternalAppIdContinueReading) {
    accessible_name = l10n_util::GetStringFUTF16(
        IDS_APP_LIST_CONTINUE_READING_ACCESSIBILE_NAME, accessible_name);
  }
  SetAccessibleName(accessible_name);
}

void SearchResultSuggestionChipView::InitLayout() {
  layout_manager_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets::TLBR(0, kPaddingDip, 0, kPaddingDip), kIconMarginDip));

  layout_manager_->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Icon.
  const int icon_size =
      SharedAppListConfig::instance().suggestion_chip_icon_dimension();
  icon_view_ = AddChildView(std::make_unique<views::ImageView>());
  icon_view_->SetImageSize(gfx::Size(icon_size, icon_size));
  icon_view_->SetPreferredSize(gfx::Size(icon_size, icon_size));

  icon_view_->SetVisible(false);

  // Text.
  text_view_ = AddChildView(std::make_unique<views::Label>());
  text_view_->SetAutoColorReadabilityEnabled(false);
  text_view_->SetSubpixelRenderingEnabled(false);
  text_view_->SetFontList(SharedAppListConfig::instance()
                              .search_result_recommendation_title_font());
  SetText(std::u16string());
  text_view_->SetEnabledColor(
      AppListColorProvider::Get()->GetSuggestionChipTextColor());
}

void SearchResultSuggestionChipView::OnButtonPressed(const ui::Event& event) {
  DCHECK(result());
  LogAppLaunch(index_in_container());
  RecordSearchResultOpenSource(result(), view_delegate_->GetAppListViewState(),
                               view_delegate_->IsInTabletMode());
  view_delegate_->OpenSearchResult(
      result()->id(), event.flags(),
      AppListLaunchedFrom::kLaunchedFromSuggestionChip,
      AppListLaunchType::kAppSearchResult, index_in_container(),
      false /* launch_as_default */);
}

void SearchResultSuggestionChipView::SetRoundedCornersForLayer(
    float corner_radius) {
  layer()->SetRoundedCornerRadius(
      {corner_radius, corner_radius, corner_radius, corner_radius});
  layer()->SetIsFastRoundedCorner(true);
}

}  // namespace ash
