// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_items_container_view.h"

#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/shell.h"
#include "ash/wm/window_restore/pine_constants.h"
#include "ash/wm/window_restore/pine_item_view.h"
#include "ash/wm/window_restore/pine_items_overflow_view.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout_view.h"

namespace ash {

namespace {
constexpr gfx::Insets kItemsContainerInsets = gfx::Insets::VH(15, 15);
constexpr int kItemsContainerRounding = 15;
}  // namespace

PineItemsContainerView::PineItemsContainerView(
    const PineContentsData::AppsInfos& apps_infos) {
  const int elements = static_cast<int>(apps_infos.size());
  CHECK_GT(elements, 0);

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemBaseElevated, kItemsContainerRounding));
  SetBetweenChildSpacing(pine::kItemsContainerChildSpacing);
  SetInsideBorderInsets(kItemsContainerInsets);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kStart);
  SetOrientation(views::BoxLayout::Orientation::kVertical);

  // TODO(sammiequon): Handle case where the app is not ready or installed.
  apps::AppRegistryCache* cache =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(
          Shell::Get()->session_controller()->GetActiveAccountId());
  auto* delegate = Shell::Get()->saved_desk_delegate();

  for (int i = 0; i < elements; ++i) {
    const PineContentsData::AppInfo& app_info = apps_infos[i];
    // If there are more than four elements, we will need to save the last
    // space for the overflow view to condense the remaining info.
    if (elements > pine::kMaxItems && i >= pine::kOverflowMinThreshold) {
      overflow_view_for_testing_ =
          AddChildView(std::make_unique<PineItemsOverflowView>(apps_infos));
      break;
    }

    std::u16string title = app_info.tab_title;
    // `cache` might be null in a test environment. In that case, we will
    // use an empty title.
    if (cache && title.empty()) {
      cache->ForOneApp(app_info.app_id,
                       [&title](const apps::AppUpdate& update) {
                         title = base::ASCIIToUTF16(update.Name());
                       });
    }

    // TODO(hewer|sammiequon): `PineItemView` should just take `app_info` and
    // `cache` as a constructor argument.
    PineItemView* item_view = AddChildView(std::make_unique<PineItemView>(
        title, app_info.tab_urls, app_info.tab_count));

    // The callback may be called synchronously.
    delegate->GetIconForAppId(app_info.app_id, pine::kAppImageSize,
                              base::BindOnce(
                                  [](base::WeakPtr<PineItemView> item_view_ptr,
                                     const gfx::ImageSkia& icon) {
                                    if (item_view_ptr) {
                                      item_view_ptr->image_view()->SetImage(
                                          ui::ImageModel::FromImageSkia(icon));
                                    }
                                  },
                                  item_view->GetWeakPtr()));
  }
}

BEGIN_METADATA(PineItemsContainerView)
END_METADATA

}  // namespace ash
