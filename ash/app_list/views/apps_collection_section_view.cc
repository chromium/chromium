// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/apps_collection_section_view.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_collections_constants.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_item_list.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/bubble/simple_grid_layout.h"
#include "ash/style/typography.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

// The margin for the section's title.
constexpr auto kTitleLabelPadding = gfx::Insets::TLBR(12, 16, 4, 16);

// The margin for the apps grid that holds each view.
constexpr auto kAppsGridPadding = gfx::Insets::TLBR(0, 8, 8, 8);

// The background corner radius for the view
constexpr int kCornerRadius = 16;

// The number of preferred apps per column for the grid view.
constexpr size_t kAppsPerColumn = 5;

std::vector<AppListItem*> GetAppListItemsForCollection(
    AppListModel* model,
    AppCollection collection_id) {
  std::vector<AppListItem*> collection;

  AppListItemList* items = model->top_level_item_list();

  for (size_t i = 0; i < items->item_count(); i++) {
    AppListItem* app_item = items->item_at(i);
    if (app_item->is_folder()) {
      // Ignore app folder items.
      continue;
    }
    if (app_item->app_status() != AppStatus::kReady) {
      // Ignore apps that are not ready.
      continue;
    }
    if (app_item->collection_id() == collection_id) {
      collection.emplace_back(app_item);
    }
  }
  return collection;
}

}  // namespace

AppsCollectionSectionView::AppsCollectionSectionView(
    AppCollection collection,
    AppListViewDelegate* view_delegate,
    AppListItemViewGridDelegate* grid_delegate)
    : collection_(collection),
      view_delegate_(view_delegate),
      grid_delegate_(grid_delegate) {
  DCHECK(view_delegate_);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kStart);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  views::Label* label = AddChildView(
      std::make_unique<views::Label>(GetAppCollectionName(collection)));
  bubble_utils::ApplyStyle(label, TypographyToken::kCrosButton2,
                           cros_tokens::kCrosSysOnSurfaceVariant);
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  label->SetBorder(views::CreateEmptyBorder(kTitleLabelPadding));

  apps_container_ = AddChildView(std::make_unique<views::View>());
  apps_container_->SetProperty(views::kMarginsKey, kAppsGridPadding);
  apps_container_->SetLayoutManager(
      std::make_unique<SimpleGridLayout>(kAppsPerColumn));

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemOnBase, kCornerRadius));
}

AppsCollectionSectionView::~AppsCollectionSectionView() {
  if (model_) {
    model_->RemoveObserver(this);
  }
}

void AppsCollectionSectionView::UpdateAppListConfig(
    const AppListConfig* app_list_config) {
  app_list_config_ = app_list_config;

  for (size_t i = 0; i < item_views_.view_size(); ++i) {
    AppListItemView* view = item_views_.view_at(i);
    view->UpdateAppListConfig(app_list_config);
  }
  InvalidateLayout();
}

void AppsCollectionSectionView::UpdateAppsForCollection() {
  if (!model_) {
    return;
  }

  DCHECK(app_list_config_);
  item_views_.Clear();
  apps_container_->RemoveAllChildViews();

  std::vector<AppListItem*> apps =
      GetAppListItemsForCollection(model_, collection_);

  for (AppListItem* app : apps) {
    CreateAndAddAppItemView(app);
  }

  SetVisible(!apps.empty());

  NotifyAccessibilityEvent(ax::mojom::Event::kChildrenChanged,
                           /*send_native_event=*/true);
  PreferredSizeChanged();
}

void AppsCollectionSectionView::SetModel(AppListModel* model) {
  if (model_ == model) {
    return;
  }

  model_ = model;

  if (model_) {
    model_->AddObserver(this);
  }

  UpdateAppsForCollection();
}

size_t AppsCollectionSectionView::GetItemViewCount() const {
  return item_views_.view_size();
}

std::optional<size_t> AppsCollectionSectionView::GetViewIndexForItem(
    const std::string& item_id) {
  for (size_t i = 0; i < item_views_.view_size(); ++i) {
    if (item_views_.view_at(i)->item()->id() == item_id) {
      return i;
    }
  }
  return std::nullopt;
}

void AppsCollectionSectionView::OnAppListModelStatusChanged() {
  UpdateAppsForCollection();
}

void AppsCollectionSectionView::OnAppListItemAdded(AppListItem* item) {
  if (item->is_folder()) {
    // Ignore app folder items.
    return;
  }
  if (item->app_status() != AppStatus::kReady) {
    // Ignore apps that are not ready.
    return;
  }
  if (item->collection_id() == collection_) {
    CreateAndAddAppItemView(item);
    PreferredSizeChanged();
  }
}

void AppsCollectionSectionView::OnAppListItemWillBeDeleted(AppListItem* item) {
  if (item->collection_id() != collection_) {
    return;
  }

  std::optional<size_t> index_to_be_deleted = GetViewIndexForItem(item->id());

  if (index_to_be_deleted) {
    AppListItemView* view = item_views_.view_at(index_to_be_deleted.value());
    item_views_.Remove(index_to_be_deleted.value());
    delete view;
    PreferredSizeChanged();
  }
}

void AppsCollectionSectionView::CreateAndAddAppItemView(AppListItem* item) {
  auto* item_view =
      apps_container_->AddChildView(std::make_unique<AppListItemView>(
          app_list_config_, grid_delegate_.get(), item, view_delegate_,
          AppListItemView::Context::kAppsCollection));
  item_view->UpdateAppListConfig(app_list_config_);
  item_views_.Add(item_view, item_views_.view_size());
  item_view->InitializeIconLoader();
}

BEGIN_METADATA(AppsCollectionSectionView)
END_METADATA

}  // namespace ash
