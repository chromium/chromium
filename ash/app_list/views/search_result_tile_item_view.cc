// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_tile_item_view.h"

#include <utility>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/model/app_list_view_state.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/pagination_model.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/focus/focus_manager.h"

namespace app_list {

namespace {

constexpr int kSearchTileWidth = 80;
constexpr int kSearchTileTopPadding = 4;
constexpr int kSearchTitleSpacing = 5;
constexpr int kSearchPriceSize = 37;
constexpr int kSearchRatingSize = 26;
constexpr int kSearchRatingStarSize = 12;
constexpr int kSearchRatingStarHorizontalSpacing = 1;
constexpr int kSearchRatingStarVerticalSpacing = 2;

// Delta applied to the font size of SearchResultTile rating.
constexpr int kSearchRatingTextSizeDelta = 1;
// Delta applied to the font size of SearchResultTile price.
constexpr int kSearchPriceTextSizeDelta = 1;

constexpr int kIconSelectedSize = 56;
constexpr int kIconSelectedCornerRadius = 4;
// Icon selected color, Google Grey 900 8%.
constexpr int kIconSelectedColor = SkColorSetA(gfx::kGoogleGrey900, 0x14);

constexpr SkColor kSearchTitleColor = SkColorSetARGB(0xDF, 0x00, 0x00, 0x00);
constexpr SkColor kSearchAppRatingColor =
    SkColorSetARGB(0x8F, 0x00, 0x00, 0x00);
constexpr SkColor kSearchAppPriceColor = SkColorSetARGB(0xFF, 0x0F, 0x9D, 0x58);
constexpr SkColor kSearchRatingStarColor =
    SkColorSetARGB(0x8F, 0x00, 0x00, 0x00);

}  // namespace

SearchResultTileItemView::SearchResultTileItemView(
    AppListViewDelegate* view_delegate,
    PaginationModel* pagination_model,
    bool show_in_apps_page)
    : view_delegate_(view_delegate),
      pagination_model_(pagination_model),
      is_play_store_app_search_enabled_(
          app_list_features::IsPlayStoreAppSearchEnabled()),
      show_in_apps_page_(show_in_apps_page),
      weak_ptr_factory_(this) {
  SetFocusBehavior(FocusBehavior::ALWAYS);

  // When |item_| is null, the tile is invisible. Calling SetSearchResult with a
  // non-null item makes the tile visible.
  SetVisible(false);

  // Prevent the icon view from interfering with our mouse events.
  icon_ = new views::ImageView;
  icon_->set_can_process_events_within_subtree(false);
  icon_->SetVerticalAlignment(views::ImageView::LEADING);
  AddChildView(icon_);

  if (is_play_store_app_search_enabled_ ||
      app_list_features::IsAppShortcutSearchEnabled()) {
    badge_ = new views::ImageView;
    badge_->set_can_process_events_within_subtree(false);
    badge_->SetVerticalAlignment(views::ImageView::LEADING);
    badge_->SetVisible(false);
    AddChildView(badge_);
  }

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title_ = new views::Label;
  title_->SetAutoColorReadabilityEnabled(false);
  title_->SetEnabledColor(AppListConfig::instance().grid_title_color());
  title_->SetFontList(rb.GetFontList(kItemTextFontStyle));
  title_->SetHorizontalAlignment(gfx::ALIGN_CENTER);
  title_->SetHandlesTooltips(false);
  AddChildView(title_);

  if (is_play_store_app_search_enabled_) {
    const gfx::FontList& font = AppListConfig::instance().app_title_font();
    rating_ = new views::Label;
    rating_->SetEnabledColor(kSearchAppRatingColor);
    rating_->SetFontList(font);
    rating_->SetLineHeight(font.GetHeight());
    rating_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
    rating_->SetVisible(false);
    AddChildView(rating_);

    rating_star_ = new views::ImageView;
    rating_star_->set_can_process_events_within_subtree(false);
    rating_star_->SetVerticalAlignment(views::ImageView::LEADING);
    rating_star_->SetImage(gfx::CreateVectorIcon(
        kIcBadgeRatingIcon, kSearchRatingStarSize, kSearchRatingStarColor));
    rating_star_->SetVisible(false);
    AddChildView(rating_star_);

    price_ = new views::Label;
    price_->SetEnabledColor(kSearchAppPriceColor);
    price_->SetFontList(font);
    price_->SetLineHeight(font.GetHeight());
    price_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    price_->SetVisible(false);
    AddChildView(price_);
  }

  set_context_menu_controller(this);
}

SearchResultTileItemView::~SearchResultTileItemView() {
  if (item_)
    item_->RemoveObserver(this);
}

void SearchResultTileItemView::SetSearchResult(SearchResult* item) {
  // Handle the case where this may be called from a nested run loop while its
  // context menu is showing. This cancels the menu (it's for the old item).
  context_menu_.reset();

  SetVisible(!!item);

  SearchResult* old_item = item_;
  if (old_item)
    old_item->RemoveObserver(this);

  item_ = item;

  if (!item)
    return;

  item_->AddObserver(this);

  SetTitle(item_->title());
  SetRating(item_->rating());
  SetPrice(item_->formatted_price());

  const gfx::FontList& font = AppListConfig::instance().app_title_font();
  if (IsSuggestedAppTileShownInAppPage()) {
    title_->SetFontList(font);
    title_->SetLineHeight(font.GetHeight());
    title_->SetEnabledColor(AppListConfig::instance().grid_title_color());
  } else {
    // Set solid color background to avoid broken text. See crbug.com/746563.
    if (rating_) {
      rating_->SetBackground(
          views::CreateSolidBackground(kCardBackgroundColor));
      if (!IsSuggestedAppTile()) {
        // App search results use different fonts than AppList apps.
        rating_->SetFontList(
            ui::ResourceBundle::GetSharedInstance()
                .GetFontList(kSearchResultTitleFontStyle)
                .DeriveWithSizeDelta(kSearchRatingTextSizeDelta));
        rating_->SetLineHeight(rating_->font_list().GetHeight());
      } else {
        rating_->SetFontList(font);
        rating_->SetLineHeight(font.GetHeight());
      }
    }
    if (price_) {
      price_->SetBackground(views::CreateSolidBackground(kCardBackgroundColor));
      if (!IsSuggestedAppTile()) {
        // App search results use different fonts than AppList apps.
        price_->SetFontList(
            ui::ResourceBundle::GetSharedInstance()
                .GetFontList(kSearchResultTitleFontStyle)
                .DeriveWithSizeDelta(kSearchPriceTextSizeDelta));
        price_->SetLineHeight(price_->font_list().GetHeight());
      } else {
        price_->SetFontList(font);
        price_->SetLineHeight(font.GetHeight());
      }
    }
    title_->SetBackground(views::CreateSolidBackground(kCardBackgroundColor));
    if (!IsSuggestedAppTile()) {
      // App search results use different fonts than AppList apps.
      title_->SetFontList(
          ui::ResourceBundle::GetSharedInstance()
              .GetFontList(kSearchResultTitleFontStyle)
              .DeriveWithSizeDelta(kSearchResultTitleTextSizeDelta));
      title_->SetLineHeight(title_->font_list().GetHeight());
    } else {
      title_->SetFontList(font);
      title_->SetLineHeight(font.GetHeight());
    }
    title_->SetEnabledColor(kSearchTitleColor);
  }

  title_->SetMaxLines(2);
  title_->SetMultiLine(
      (item_->display_type() == ash::SearchResultDisplayType::kTile ||
       (IsSuggestedAppTile() && !show_in_apps_page_)) &&
      item_->result_type() == ash::SearchResultType::kInstalledApp);

  // If the new icon is null, it's being decoded asynchronously. Not updating it
  // now to prevent flickering from showing an empty icon while decoding.
  if (!item->icon().isNull())
    OnMetadataChanged();

  base::string16 accessible_name;
  if (!item_->accessible_name().empty())
    accessible_name = item_->accessible_name();
  else
    accessible_name = title_->text();

  if (rating_ && rating_->visible()) {
    accessible_name +=
        base::UTF8ToUTF16(", ") +
        l10n_util::GetStringFUTF16(IDS_APP_ACCESSIBILITY_STAR_RATING_ARC,
                                   rating_->text());
  }
  if (price_ && price_->visible())
    accessible_name += base::UTF8ToUTF16(", ") + price_->text();
  SetAccessibleName(accessible_name);
}

void SearchResultTileItemView::SetParentBackgroundColor(SkColor color) {
  parent_background_color_ = color;
  UpdateBackgroundColor();
}

void SearchResultTileItemView::ButtonPressed(views::Button* sender,
                                             const ui::Event& event) {
  if (IsSuggestedAppTile())
    LogAppLaunch();

  RecordSearchResultOpenSource(item_, view_delegate_->GetModel(),
                               view_delegate_->GetSearchModel());
  view_delegate_->OpenSearchResult(item_->id(), event.flags());
}

void SearchResultTileItemView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  views::Button::GetAccessibleNodeData(node_data);
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
  // Return early if |item_| was deleted due to the search result list changing.
  // see crbug.com/801142
  if (!item_)
    return true;

  if (event.key_code() == ui::VKEY_RETURN) {
    if (IsSuggestedAppTile())
      LogAppLaunch();

    RecordSearchResultOpenSource(item_, view_delegate_->GetModel(),
                                 view_delegate_->GetSearchModel());
    view_delegate_->OpenSearchResult(item_->id(), event.flags());
    return true;
  }

  return false;
}

void SearchResultTileItemView::OnFocus() {
  if (pagination_model_ && IsSuggestedAppTile() &&
      view_delegate_->GetModel()->state() == ash::AppListState::kStateApps) {
    // Go back to first page when app in suggestions container is focused.
    pagination_model_->SelectPage(0, false);
  } else {
    ScrollRectToVisible(GetLocalBounds());
  }
  SetBackgroundHighlighted(true);
  UpdateBackgroundColor();
  NotifyAccessibilityEvent(ax::mojom::Event::kSelection, true);
}

void SearchResultTileItemView::OnBlur() {
  SetBackgroundHighlighted(false);
  UpdateBackgroundColor();
}

void SearchResultTileItemView::StateChanged(ButtonState old_state) {
  UpdateBackgroundColor();
}

void SearchResultTileItemView::PaintButtonContents(gfx::Canvas* canvas) {
  if (!item_ || !background_highlighted())
    return;

  gfx::Rect rect(GetContentsBounds());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  if (IsSuggestedAppTileShownInAppPage()) {
    rect.ClampToCenteredSize(AppListConfig::instance().grid_focus_size());
    flags.setColor(kGridSelectedColor);
    canvas->DrawRoundRect(gfx::RectF(rect),
                          AppListConfig::instance().grid_focus_corner_radius(),
                          flags);
  } else {
    const int kLeftRightPadding = (rect.width() - kIconSelectedSize) / 2;
    rect.Inset(kLeftRightPadding, 0);
    rect.set_height(kIconSelectedSize);
    flags.setColor(kIconSelectedColor);
    canvas->DrawRoundRect(gfx::RectF(rect), kIconSelectedCornerRadius, flags);
  }
}

void SearchResultTileItemView::OnMetadataChanged() {
  SetIcon(item_->icon());
  SetTitle(item_->title());
  SetBadgeIcon(item_->badge_icon());
  SetRating(item_->rating());
  SetPrice(item_->formatted_price());
  Layout();
}

void SearchResultTileItemView::OnResultDestroying() {
  // The menu comes from |item_|. If we're showing a menu we need to cancel it.
  context_menu_.reset();

  if (item_)
    item_->RemoveObserver(this);

  SetSearchResult(nullptr);
}

void SearchResultTileItemView::ShowContextMenuForView(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type) {
  // |item_| could be null when result list is changing.
  if (!item_)
    return;

  view_delegate_->GetSearchResultContextMenuModel(
      item_->id(),
      base::BindOnce(&SearchResultTileItemView::OnGetContextMenuModel,
                     weak_ptr_factory_.GetWeakPtr(), source, point,
                     source_type));
}

void SearchResultTileItemView::OnGetContextMenuModel(
    views::View* source,
    const gfx::Point& point,
    ui::MenuSourceType source_type,
    std::vector<ash::mojom::MenuItemPtr> menu) {
  if (menu.empty() || (context_menu_ && context_menu_->IsShowingMenu()))
    return;

  int run_types = views::MenuRunner::HAS_MNEMONICS;
  views::MenuAnchorPosition anchor_position = views::MENU_ANCHOR_TOPLEFT;
  gfx::Rect anchor_rect = gfx::Rect(point, gfx::Size());

  if (::features::IsTouchableAppContextMenuEnabled()) {
    anchor_position = views::MENU_ANCHOR_BUBBLE_TOUCHABLE_RIGHT;
    run_types |= views::MenuRunner::USE_TOUCHABLE_LAYOUT |
                 views::MenuRunner::CONTEXT_MENU |
                 views::MenuRunner::FIXED_ANCHOR;
    anchor_rect = source->GetBoundsInScreen();
    // Anchor the menu to the same rect that is used for selection highlight.
    anchor_rect.ClampToCenteredSize(
        AppListConfig::instance().grid_focus_size());
  }

  context_menu_ = std::make_unique<AppListMenuModelAdapter>(
      item_->id(), this, source_type, this, GetAppType(),
      base::BindOnce(&SearchResultTileItemView::OnMenuClosed,
                     weak_ptr_factory_.GetWeakPtr()));
  context_menu_->Build(std::move(menu));
  context_menu_->Run(anchor_rect, anchor_position, run_types);
  source->RequestFocus();
}

void SearchResultTileItemView::OnMenuClosed() {
  OnBlur();
}

void SearchResultTileItemView::ExecuteCommand(int command_id, int event_flags) {
  if (item_) {
    view_delegate_->SearchResultContextMenuItemSelected(item_->id(), command_id,
                                                        event_flags);
  }
}

void SearchResultTileItemView::SetIcon(const gfx::ImageSkia& icon) {
  const int icon_size = AppListConfig::instance().search_tile_icon_dimension();
  gfx::ImageSkia resized(gfx::ImageSkiaOperations::CreateResizedImage(
      icon, skia::ImageOperations::RESIZE_BEST,
      gfx::Size(icon_size, icon_size)));
  icon_->SetImage(resized);
}

void SearchResultTileItemView::SetBadgeIcon(const gfx::ImageSkia& badge_icon) {
  if (!badge_)
    return;

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
  title_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
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
        AppListViewState::PEEKING) {
      return AppListMenuModelAdapter::PEEKING_SUGGESTED;
    } else {
      return AppListMenuModelAdapter::FULLSCREEN_SUGGESTED;
    }
  } else {
    if (view_delegate_->GetModel()->state_fullscreen() ==
        AppListViewState::HALF) {
      return AppListMenuModelAdapter::HALF_SEARCH_RESULT;
    } else if (view_delegate_->GetModel()->state_fullscreen() ==
               AppListViewState::FULLSCREEN_SEARCH) {
      return AppListMenuModelAdapter::FULLSCREEN_SEARCH_RESULT;
    }
  }
  NOTREACHED();
  return AppListMenuModelAdapter::APP_LIST_APP_TYPE_LAST;
}

bool SearchResultTileItemView::IsSuggestedAppTile() const {
  return item_ &&
         item_->display_type() == ash::SearchResultDisplayType::kRecommendation;
}

bool SearchResultTileItemView::IsSuggestedAppTileShownInAppPage() const {
  return IsSuggestedAppTile() && show_in_apps_page_;
}

void SearchResultTileItemView::LogAppLaunch() const {
  // Only log the app launch if the class is being used as a suggested app.
  if (!IsSuggestedAppTile())
    return;

  UMA_HISTOGRAM_BOOLEAN(kAppListAppLaunchedFullscreen,
                        true /* suggested app */);
  base::RecordAction(base::UserMetricsAction("AppList_OpenSuggestedApp"));
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
  if (rect.IsEmpty() || !item_)
    return;

  if (IsSuggestedAppTileShownInAppPage()) {
    icon_->SetBoundsRect(AppListItemView::GetIconBoundsForTargetViewBounds(
        rect, icon_->GetImage().size()));
    title_->SetBoundsRect(AppListItemView::GetTitleBoundsForTargetViewBounds(
        rect, title_->GetPreferredSize()));
  } else {
    gfx::Rect icon_rect(rect);
    icon_rect.ClampToCenteredSize(icon_->GetImage().size());
    icon_rect.set_y(kSearchTileTopPadding);
    icon_->SetBoundsRect(icon_rect);

    if (badge_) {
      const int badge_icon_dimension =
          AppListConfig::instance().search_tile_badge_icon_dimension();
      const int badge_icon_offset =
          AppListConfig::instance().search_tile_badge_icon_offset();
      const gfx::Rect badge_rect(
          icon_rect.right() - badge_icon_dimension + badge_icon_offset,
          icon_rect.bottom() - badge_icon_dimension + badge_icon_offset,
          badge_icon_dimension, badge_icon_dimension);
      badge_->SetBoundsRect(badge_rect);
    }

    rect.set_y(icon_rect.bottom() + kSearchTitleSpacing);
    rect.set_height(title_->GetPreferredSize().height());
    title_->SetBoundsRect(rect);

    if (rating_) {
      gfx::Rect rating_rect(rect);
      rating_rect.Inset(0, title_->GetPreferredSize().height(), 0, 0);
      rating_rect.set_size(rating_->GetPreferredSize());
      rating_rect.set_width(kSearchRatingSize);
      rating_->SetBoundsRect(rating_rect);
    }

    if (rating_star_) {
      gfx::Rect rating_star_rect(rect);
      rating_star_rect.Inset(
          kSearchRatingSize + kSearchRatingStarHorizontalSpacing,
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
  if (!item_)
    return gfx::Size();

  if (IsSuggestedAppTileShownInAppPage()) {
    return gfx::Size(AppListConfig::instance().grid_tile_width(),
                     AppListConfig::instance().grid_tile_height());
  }

  return gfx::Size(kSearchTileWidth, kSearchTileHeight);
}

bool SearchResultTileItemView::GetTooltipText(const gfx::Point& p,
                                              base::string16* tooltip) const {
  // Use the label to generate a tooltip, so that it will consider its text
  // truncation in making the tooltip. We do not want the label itself to have a
  // tooltip, so we only temporarily enable it to get the tooltip text from the
  // label, then disable it again.
  title_->SetHandlesTooltips(true);
  bool handled = title_->GetTooltipText(p, tooltip);
  title_->SetHandlesTooltips(false);
  return handled;
}

}  // namespace app_list
