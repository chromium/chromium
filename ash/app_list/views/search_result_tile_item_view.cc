// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_tile_item_view.h"

#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/public/cpp/app_list/app_list_color_provider.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/focus/focus_manager.h"

namespace ash {

namespace {

constexpr int kSearchTileWidth = 80;
constexpr int kSearchTileTopPadding = 4;
constexpr int kSearchTitleSpacing = 7;
constexpr int kSearchPriceSize = 37;
constexpr int kSearchRatingSize = 26;
constexpr int kSearchRatingStarSize = 12;
constexpr int kSearchRatingStarHorizontalSpacing = 1;
constexpr int kSearchRatingStarVerticalSpacing = 2;
// Text line height in the search result tile.
constexpr int kTileTextLineHeight = 16;

// Delta applied to the font size of SearchResultTile title.
constexpr int kSearchResultTileTitleTextSizeDelta = 1;

constexpr int kIconSelectedSize = 56;
constexpr int kIconSelectedCornerRadius = 4;

// Offset for centering star rating when there is no price.
constexpr int kSearchRatingCenteringOffset =
    ((kSearchTileWidth -
      (kSearchRatingSize + kSearchRatingStarHorizontalSpacing +
       kSearchRatingStarSize)) /
     2);

}  // namespace

SearchResultTileItemView::SearchResultTileItemView(
    AppListViewDelegate* view_delegate,
    bool show_in_apps_page)
    : view_delegate_(view_delegate),
      is_app_reinstall_recommendation_enabled_(
          app_list_features::IsAppReinstallZeroStateEnabled()),
      show_in_apps_page_(show_in_apps_page) {
  SetFocusBehavior(FocusBehavior::ALWAYS);

  // When |result_| is null, the tile is invisible. Calling SetSearchResult with
  // a non-null item makes the tile visible.
  SetVisible(false);

  GetViewAccessibility().OverrideIsLeaf(true);

  // Prevent the icon view from interfering with our mouse events.
  icon_ = new views::ImageView;
  icon_->SetCanProcessEventsWithinSubtree(false);
  icon_->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  AddChildView(icon_);

  badge_ = new views::ImageView;
  badge_->SetCanProcessEventsWithinSubtree(false);
  badge_->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  badge_->SetVisible(false);
  AddChildView(badge_);

  title_ = new views::Label;
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetEnabledColor(AppListColorProvider::Get()->GetSearchBoxTextColor(
      /*default_color*/ SK_ColorWHITE));
  title_->SetLineHeight(kTileTextLineHeight);
  title_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  title_->SetHandlesTooltips(false);
  title_->SetAllowCharacterBreak(true);
  AddChildView(title_);

  rating_ = new views::Label;
  rating_->SetEnabledColor(
      AppListColorProvider::Get()->GetSearchBoxSecondaryTextColor(
          /*default_color*/ gfx::kGoogleGrey700));
  rating_->SetLineHeight(kTileTextLineHeight);
  rating_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  rating_->SetVisible(false);
  AddChildView(rating_);

  rating_star_ = new views::ImageView;
  rating_star_->SetCanProcessEventsWithinSubtree(false);
  rating_star_->SetVerticalAlignment(views::ImageView::Alignment::kLeading);
  rating_star_->SetImage(gfx::CreateVectorIcon(
      kBadgeRatingIcon, kSearchRatingStarSize,
      AppListColorProvider::Get()->GetSearchBoxSecondaryTextColor(
          gfx::kGoogleGrey700)));
  rating_star_->SetVisible(false);
  AddChildView(rating_star_);

  price_ = new views::Label;
  price_->SetEnabledColor(
      AppListColorProvider::Get()->GetSearchBoxSecondaryTextColor(
          /*default_color*/ gfx::kGoogleGreen600));
  price_->SetLineHeight(kTileTextLineHeight);
  price_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  price_->SetVisible(false);
  AddChildView(price_);

  set_context_menu_controller(this);
}

SearchResultTileItemView::~SearchResultTileItemView() = default;

void SearchResultTileItemView::OnResultChanged() {
  // Handle the case where this may be called from a nested run loop while its
  // context menu is showing. This cancels the menu (it's for the old item).
  context_menu_.reset();

  SetVisible(!!result());

  if (!result())
    return;

  SetTitle(result()->title());
  SetRating(result()->rating());
  SetPrice(result()->formatted_price());

  const gfx::FontList& font = AppListConfig::instance().app_title_font();
  if (IsSuggestedAppTileShownInAppPage()) {
    title_->SetFontList(font);
    title_->SetEnabledColor(AppListConfig::instance().grid_title_color());
  } else {
    // Set solid color background to avoid broken text. See crbug.com/746563.
    if (rating_) {
      rating_->SetBackground(views::CreateSolidBackground(
          AppListColorProvider::Get()->GetSearchBoxCardBackgroundColor()));
      if (!IsSuggestedAppTile()) {
        // App search results use different fonts than AppList apps.
        rating_->SetFontList(
            ui::ResourceBundle::GetSharedInstance().GetFontList(
                AppListConfig::instance().search_result_title_font_style()));
      } else {
        rating_->SetFontList(font);
      }
    }
    if (price_) {
      price_->SetBackground(views::CreateSolidBackground(
          AppListColorProvider::Get()->GetSearchBoxCardBackgroundColor()));
      if (!IsSuggestedAppTile()) {
        // App search results use different fonts than AppList apps.
        price_->SetFontList(ui::ResourceBundle::GetSharedInstance().GetFontList(
            AppListConfig::instance().search_result_title_font_style()));
      } else {
        price_->SetFontList(font);
      }
    }
    title_->SetBackground(views::CreateSolidBackground(
        AppListColorProvider::Get()->GetSearchBoxCardBackgroundColor()));
    if (!IsSuggestedAppTile()) {
      // App search results use different fonts than AppList apps.
      title_->SetFontList(
          ui::ResourceBundle::GetSharedInstance()
              .GetFontList(
                  AppListConfig::instance().search_result_title_font_style())
              .DeriveWithSizeDelta(kSearchResultTileTitleTextSizeDelta));
    } else {
      title_->SetFontList(font);
    }
    title_->SetEnabledColor(AppListColorProvider::Get()->GetSearchBoxTextColor(
        /*default_color*/ gfx::kGoogleGrey900));
  }

  title_->SetMaxLines(2);
  title_->SetMultiLine(
      (result()->display_type() == SearchResultDisplayType::kTile ||
       (IsSuggestedAppTile() && !show_in_apps_page_)) &&
      (result()->result_type() == AppListSearchResultType::kInstalledApp ||
       result()->result_type() == AppListSearchResultType::kArcAppShortcut));

  // If the new icon is null, it's being decoded asynchronously. Not updating it
  // now to prevent flickering from showing an empty icon while decoding.
  if (!result()->icon().isNull())
    OnMetadataChanged();

  UpdateAccessibleName();
}

base::string16 SearchResultTileItemView::ComputeAccessibleName() const {
  base::string16 accessible_name;
  if (!result()->accessible_name().empty())
    return result()->accessible_name();

  if (result()->result_type() == AppListSearchResultType::kPlayStoreApp ||
      result()->result_type() == AppListSearchResultType::kInstantApp) {
    accessible_name = l10n_util::GetStringFUTF16(
        IDS_APP_ACCESSIBILITY_ARC_APP_ANNOUNCEMENT, title_->GetText());
  } else if (result()->result_type() ==
             AppListSearchResultType::kPlayStoreReinstallApp) {
    accessible_name = l10n_util::GetStringFUTF16(
        IDS_APP_ACCESSIBILITY_APP_RECOMMENDATION_ARC, title_->GetText());
  } else if (result()->result_type() ==
             AppListSearchResultType::kInstalledApp) {
    accessible_name = l10n_util::GetStringFUTF16(
        IDS_APP_ACCESSIBILITY_INSTALLED_APP_ANNOUNCEMENT, title_->GetText());
  } else if (result()->result_type() == AppListSearchResultType::kInternalApp) {
    accessible_name = l10n_util::GetStringFUTF16(
        IDS_APP_ACCESSIBILITY_INTERNAL_APP_ANNOUNCEMENT, title_->GetText());
  } else {
    accessible_name = title_->GetText();
  }

  if (rating_ && rating_->GetVisible()) {
    accessible_name = l10n_util::GetStringFUTF16(
        IDS_APP_ACCESSIBILITY_APP_WITH_STAR_RATING_ARC, accessible_name,
        rating_->GetText());
  }
  if (price_ && price_->GetVisible()) {
    accessible_name =
        l10n_util::GetStringFUTF16(IDS_APP_ACCESSIBILITY_APP_WITH_PRICE_ARC,
                                   accessible_name, price_->GetText());
  }
  return accessible_name;
}

void SearchResultTileItemView::SetParentBackgroundColor(SkColor color) {
  parent_background_color_ = color;
  UpdateBackgroundColor();
}

void SearchResultTileItemView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  ActivateResult(event.flags(), true /* by_button_press */);
}

void SearchResultTileItemView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  views::Button::GetAccessibleNodeData(node_data);

  // The tile is a list item in the search result page's result list.
  node_data->role = ax::mojom::Role::kListBoxOption;
  node_data->AddBoolAttribute(ax::mojom::BoolAttribute::kSelected, selected());
  node_data->SetDefaultActionVerb(ax::mojom::DefaultActionVerb::kClick);

  // Specify |ax::mojom::StringAttribute::kDescription| with an empty string, so
  // that long truncated names are not read twice. Details of this issue: - The
  // Play Store app's name is shown in a label |title_|. - If the name is too
  // long, it'll get truncated and the full name will
  //   go to the label's tooltip.
  // - SearchResultTileItemView uses that label's tooltip as its tooltip.
  // - If a view doesn't have |ax::mojom::StringAttribute::kDescription| defined
  // in the
  //   |AXNodeData|, |AXViewObjWrapper::Serialize| will use the tooltip text
  //   as its description.
  // - We're customizing this view's accessible name, so it get focused
  //   ChromeVox will read its accessible name and then its description.
  node_data->AddStringAttribute(ax::mojom::StringAttribute::kDescription, "");
}

bool SearchResultTileItemView::OnKeyPressed(const ui::KeyEvent& event) {
  // Return early if |result()| was deleted due to the search result list
  // changing. see crbug.com/801142
  if (!result())
    return true;

  if (event.key_code() == ui::VKEY_RETURN) {
    ActivateResult(event.flags(), false /* by_button_press */);
    return true;
  }
  return false;
}

void SearchResultTileItemView::StateChanged(ButtonState old_state) {
  SearchResultBaseView::StateChanged(old_state);
  UpdateBackgroundColor();
}

void SearchResultTileItemView::PaintButtonContents(gfx::Canvas* canvas) {
  if (!result() || !selected())
    return;

  gfx::Rect rect(GetContentsBounds());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  if (IsSuggestedAppTileShownInAppPage()) {
    rect.ClampToCenteredSize(AppListConfig::instance().grid_focus_size());
    flags.setColor(
        AppListColorProvider::Get()->GetSearchResultViewInkDropColor());
    canvas->DrawRoundRect(gfx::RectF(rect),
                          AppListConfig::instance().grid_focus_corner_radius(),
                          flags);
  } else {
    const int kLeftRightPadding = (rect.width() - kIconSelectedSize) / 2;
    rect.Inset(kLeftRightPadding, 0);
    rect.set_height(kIconSelectedSize);
    flags.setColor(
        AppListColorProvider::Get()->GetSearchResultViewInkDropColor());
    canvas->DrawRoundRect(gfx::RectF(rect), kIconSelectedCornerRadius, flags);
  }
}

void SearchResultTileItemView::OnMetadataChanged() {
  SetIcon(result()->icon());
  SetTitle(result()->title());
  SetBadgeIcon(result()->badge_icon());
  SetRating(result()->rating());
  SetPrice(result()->formatted_price());
  Layout();
}

void SearchResultTileItemView::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // |result()| could be null when result list is changing.
  if (!result())
    return;

  view_delegate_->GetSearchResultContextMenuModel(
      result()->id(),
      base::BindOnce(&SearchResultTileItemView::OnGetContextMenuModel,
                     weak_ptr_factory_.GetWeakPtr(), source, point,
                     source_type));
}

void SearchResultTileItemView::OnGetContextMenuModel(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type,
    std::unique_ptr<ui::SimpleMenuModel> menu_model) {
  if (!menu_model || (context_menu_ && context_menu_->IsShowingMenu()))
    return;

  gfx::Rect anchor_rect = source->GetBoundsInScreen();
  // Anchor the menu to the same rect that is used for selection highlight.
  anchor_rect.ClampToCenteredSize(AppListConfig::instance().grid_focus_size());

  AppLaunchedMetricParams metric_params = {
      AppListLaunchedFrom::kLaunchedFromSearchBox,
      AppListLaunchType::kAppSearchResult};
  view_delegate_->GetAppLaunchedMetricParams(&metric_params);

  context_menu_ = std::make_unique<AppListMenuModelAdapter>(
      result()->id(), std::move(menu_model), GetWidget(), source_type,
      metric_params, GetAppType(),
      base::BindOnce(&SearchResultTileItemView::OnMenuClosed,
                     weak_ptr_factory_.GetWeakPtr()),
      view_delegate_->GetSearchModel()->tablet_mode());
  context_menu_->Run(anchor_rect, views::MenuAnchorPosition::kBubbleRight,
                     views::MenuRunner::HAS_MNEMONICS |
                         views::MenuRunner::USE_TOUCHABLE_LAYOUT |
                         views::MenuRunner::CONTEXT_MENU |
                         views::MenuRunner::FIXED_ANCHOR);
  if (!selected()) {
    selected_for_context_menu_ = true;
    SetSelected(true, base::nullopt);
  }
}

void SearchResultTileItemView::OnMenuClosed() {
  // Release menu since its menu model delegate (AppContextMenu) could be
  // released as a result of menu command execution.
  context_menu_.reset();
  if (selected_for_context_menu_) {
    selected_for_context_menu_ = false;
    SetSelected(false, base::nullopt);
  }
}

void SearchResultTileItemView::ActivateResult(int event_flags,
                                              bool by_button_press) {
  const bool launch_as_default = is_default_result() && !by_button_press;
  if (result()->result_type() == AppListSearchResultType::kPlayStoreApp) {
    const base::TimeDelta activation_delay =
        base::TimeTicks::Now() - result_display_start_time();
    UMA_HISTOGRAM_MEDIUM_TIMES("Arc.PlayStoreSearch.ResultClickLatency",
                               activation_delay);
    UMA_HISTOGRAM_EXACT_LINEAR(
        "Apps.AppListPlayStoreAppLaunchedIndex",
        group_index_in_container_view(),
        AppListConfig::instance().max_search_result_tiles());
    if (launch_as_default) {
      UMA_HISTOGRAM_MEDIUM_TIMES(
          "Arc.PlayStoreSearch.DefaultResultClickLatency", activation_delay);
    }
  }

  LogAppLaunchForSuggestedApp();

  RecordSearchResultOpenSource(result(), view_delegate_->GetModel(),
                               view_delegate_->GetSearchModel());
  view_delegate_->OpenSearchResult(result()->id(), event_flags,
                                   AppListLaunchedFrom::kLaunchedFromSearchBox,
                                   AppListLaunchType::kAppSearchResult,
                                   index_in_container(), launch_as_default);
}

void SearchResultTileItemView::SetIcon(const gfx::ImageSkia& icon) {
  const int icon_size = AppListConfig::instance().search_tile_icon_dimension();
  gfx::ImageSkia resized(gfx::ImageSkiaOperations::CreateResizedImage(
      icon, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(icon_size, icon_size)));
  icon_->SetImage(resized);
}

void SearchResultTileItemView::SetBadgeIcon(const gfx::ImageSkia& badge_icon) {
  if (badge_icon.isNull()) {
    badge_->SetVisible(false);
    return;
  }

  gfx::ImageSkia resized_badge_icon(
      gfx::ImageSkiaOperations::CreateResizedImage(
          badge_icon, skia::ImageOperations::RESIZE_BEST,
          AppListConfig::instance().search_tile_badge_icon_size()));

  gfx::ShadowValues shadow_values;
  shadow_values.push_back(
      gfx::ShadowValue(gfx::Vector2d(0, 1), 0, SkColorSetARGB(0x33, 0, 0, 0)));
  shadow_values.push_back(
      gfx::ShadowValue(gfx::Vector2d(0, 1), 2, SkColorSetARGB(0x33, 0, 0, 0)));
  badge_->SetImage(gfx::ImageSkiaOperations::CreateImageWithDropShadow(
      resized_badge_icon, shadow_values));
  badge_->SetVisible(true);
}

void SearchResultTileItemView::SetTitle(const base::string16& title) {
  title_->SetText(title);
}

void SearchResultTileItemView::SetRating(float rating) {
  if (!rating_)
    return;

  if (rating < 0) {
    rating_->SetVisible(false);
    rating_star_->SetVisible(false);
    return;
  }

  rating_->SetText(base::FormatDouble(rating, 1));
  rating_->SetVisible(true);
  rating_star_->SetVisible(true);
}

void SearchResultTileItemView::SetPrice(const base::string16& price) {
  if (!price_)
    return;

  if (price.empty()) {
    price_->SetVisible(false);
    return;
  }

  price_->SetText(price);
  price_->SetVisible(true);
}

AppListMenuModelAdapter::AppListViewAppType
SearchResultTileItemView::GetAppType() const {
  if (IsSuggestedAppTile()) {
    if (view_delegate_->GetModel()->state_fullscreen() ==
        AppListViewState::kPeeking) {
      return AppListMenuModelAdapter::PEEKING_SUGGESTED;
    } else {
      return AppListMenuModelAdapter::FULLSCREEN_SUGGESTED;
    }
  } else {
    if (view_delegate_->GetModel()->state_fullscreen() ==
        AppListViewState::kHalf) {
      return AppListMenuModelAdapter::HALF_SEARCH_RESULT;
    } else if (view_delegate_->GetModel()->state_fullscreen() ==
               AppListViewState::kFullscreenSearch) {
      return AppListMenuModelAdapter::FULLSCREEN_SEARCH_RESULT;
    }
  }
  NOTREACHED();
  return AppListMenuModelAdapter::APP_LIST_APP_TYPE_LAST;
}

bool SearchResultTileItemView::IsSuggestedAppTile() const {
  return result() && result()->is_recommendation();
}

bool SearchResultTileItemView::IsSuggestedAppTileShownInAppPage() const {
  return IsSuggestedAppTile() && show_in_apps_page_;
}

void SearchResultTileItemView::LogAppLaunchForSuggestedApp() const {
  // Only log the app launch if the class is being used as a suggested app.
  if (!IsSuggestedAppTile())
    return;

  // We only need to record opening the installed app in zero state, no need to
  // record the opening of a fast re-installed app, since the latter is already
  // recorded in ArcAppReinstallAppResult::Open.
  if (result()->result_type() !=
      AppListSearchResultType::kPlayStoreReinstallApp) {
    base::RecordAction(
        base::UserMetricsAction("AppList_ZeroStateOpenInstalledApp"));
  }
}

void SearchResultTileItemView::UpdateBackgroundColor() {
  // Tell the label what color it will be drawn onto. It will use whether the
  // background color is opaque or transparent to decide whether to use subpixel
  // rendering. Does not actually set the label's background color.
  title_->SetBackgroundColor(parent_background_color_);
  SchedulePaint();
}

void SearchResultTileItemView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty() || !result())
    return;

  if (IsSuggestedAppTileShownInAppPage()) {
    icon_->SetBoundsRect(AppListItemView::GetIconBoundsForTargetViewBounds(
        AppListConfig::instance(), rect, icon_->GetImage().size(),
        /*icon_scale=*/1.0f));
    title_->SetBoundsRect(AppListItemView::GetTitleBoundsForTargetViewBounds(
        AppListConfig::instance(), rect, title_->GetPreferredSize(),
        /*icon_scale=*/1.0f));
  } else {
    gfx::Rect icon_rect(rect);
    icon_rect.ClampToCenteredSize(icon_->GetImage().size());
    icon_rect.set_y(kSearchTileTopPadding);
    icon_->SetBoundsRect(icon_rect);

    const int badge_icon_dimension =
        AppListConfig::instance().search_tile_badge_icon_dimension();
    const int badge_icon_offset =
        AppListConfig::instance().search_tile_badge_icon_offset();
    const gfx::Rect badge_rect(
        icon_rect.right() - badge_icon_dimension + badge_icon_offset,
        icon_rect.bottom() - badge_icon_dimension + badge_icon_offset,
        badge_icon_dimension, badge_icon_dimension);
    badge_->SetBoundsRect(badge_rect);

    rect.set_y(icon_rect.bottom() + kSearchTitleSpacing);
    rect.set_height(title_->GetPreferredSize().height());
    title_->SetBoundsRect(rect);

    // If there is no price set, we center the rating.
    const bool center_rating =
        rating_ && rating_star_ && price_ && price_->GetText().empty();
    const int rating_horizontal_offset =
        center_rating ? kSearchRatingCenteringOffset : 0;

    if (rating_) {
      gfx::Rect rating_rect(rect);
      rating_rect.Inset(rating_horizontal_offset,
                        title_->GetPreferredSize().height(), 0, 0);
      rating_rect.set_size(rating_->GetPreferredSize());
      rating_rect.set_width(kSearchRatingSize);
      rating_->SetBoundsRect(rating_rect);
    }

    if (rating_star_) {
      gfx::Rect rating_star_rect(rect);
      rating_star_rect.Inset(rating_horizontal_offset + kSearchRatingSize +
                                 kSearchRatingStarHorizontalSpacing,
                             title_->GetPreferredSize().height() +
                                 kSearchRatingStarVerticalSpacing,
                             0, 0);
      rating_star_rect.set_size(rating_star_->GetPreferredSize());
      rating_star_->SetBoundsRect(rating_star_rect);
    }

    if (price_) {
      gfx::Rect price_rect(rect);
      price_rect.Inset(rect.width() - kSearchPriceSize,
                       title_->GetPreferredSize().height(), 0, 0);
      price_rect.set_size(price_->GetPreferredSize());
      price_->SetBoundsRect(price_rect);
    }
  }
}

const char* SearchResultTileItemView::GetClassName() const {
  return "SearchResultTileItemView";
}

gfx::Size SearchResultTileItemView::CalculatePreferredSize() const {
  if (!result())
    return gfx::Size();

  if (IsSuggestedAppTileShownInAppPage()) {
    return gfx::Size(AppListConfig::instance().grid_tile_width(),
                     AppListConfig::instance().grid_tile_height());
  }

  return gfx::Size(kSearchTileWidth,
                   AppListConfig::instance().search_tile_height());
}

base::string16 SearchResultTileItemView::GetTooltipText(
    const gfx::Point& p) const {
  // Use the label to generate a tooltip, so that it will consider its text
  // truncation in making the tooltip. We do not want the label itself to have a
  // tooltip, so we only temporarily enable it to get the tooltip text from the
  // label, then disable it again.
  title_->SetHandlesTooltips(true);
  base::string16 tooltip = title_->GetTooltipText(p);
  title_->SetHandlesTooltips(false);
  return tooltip;
}

}  // namespace ash
