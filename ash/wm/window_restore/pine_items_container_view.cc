// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_items_container_view.h"

#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/shell.h"
#include "ash/wm/window_restore/pine_constants.h"
#include "ash/wm/window_restore/pine_items_overflow_view.h"
#include "base/barrier_callback.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"

namespace ash {

namespace {

// Constants for `PineItemView`.
constexpr gfx::Size kFaviconPreferredSize(16, 16);
constexpr int kItemIconBackgroundRounding = 10;
constexpr gfx::Size kItemIconPreferredSize(32, 32);

// Constants for `PineItemsContainerView`.
constexpr gfx::Insets kItemsContainerInsets = gfx::Insets::VH(15, 15);
constexpr int kItemsContainerRounding = 15;

// Represents an app that will be shown in the pine widget. Contains the app
// title and app icon. Optionally contains a couple favicons depending on the
// app.
// TODO(sammiequon): Add ASCII art.
class PineItemView : public views::BoxLayoutView {
  METADATA_HEADER(PineItemView, views::BoxLayoutView)

 public:
  PineItemView(const std::string& app_title,
               const std::vector<std::string>& favicons) {
    SetBetweenChildSpacing(pine::kItemChildSpacing);
    SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
    SetOrientation(views::BoxLayout::Orientation::kHorizontal);

    AddChildView(views::Builder<views::ImageView>()
                     .CopyAddressTo(&image_view_)
                     .SetBackground(views::CreateRoundedRectBackground(
                         SK_ColorLTGRAY, kItemIconBackgroundRounding))
                     .SetImageSize(kItemIconPreferredSize)
                     .SetPreferredSize(pine::kItemIconBackgroundPreferredSize)
                     .Build());

    views::Label* app_title_label;
    AddChildView(views::Builder<views::Label>()
                     .CopyAddressTo(&app_title_label)
                     .SetEnabledColor(SK_ColorBLACK)
                     .SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL,
                                                pine::kItemTitleFontSize,
                                                gfx::Font::Weight::BOLD))
                     .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                     .SetText(base::ASCIIToUTF16(app_title))
                     .Build());
    SetFlexForView(app_title_label, 1);

    if (favicons.empty()) {
      return;
    }

    // Use a barrier callback so that we only layout once after all favicons are
    // added as views.
    auto barrier = base::BarrierCallback<const gfx::ImageSkia&>(
        /*num_callbacks=*/favicons.size(),
        /*done_callback=*/base::BindOnce(&PineItemView::OnAllFaviconsLoaded,
                                         weak_ptr_factory_.GetWeakPtr()));

    auto* delegate = Shell::Get()->saved_desk_delegate();
    for (const std::string& url : favicons) {
      // TODO(b/325638530): When lacros is active, this needs to supply a valid
      // profile id.
      delegate->GetFaviconForUrl(
          url, /*lacros_profile_id=*/0,
          base::BindOnce(&PineItemView::OnOneFaviconLoaded, GetWeakPtr(),
                         barrier),
          &cancelable_favicon_task_tracker_);
    }
  }

  PineItemView(const PineItemView&) = delete;
  PineItemView& operator=(const PineItemView&) = delete;
  ~PineItemView() override = default;

  views::ImageView* image_view() { return image_view_; }

  base::WeakPtr<PineItemView> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void OnOneFaviconLoaded(
      base::OnceCallback<void(const gfx::ImageSkia&)> callback,
      const gfx::ImageSkia& favicon) {
    std::move(callback).Run(favicon);
  }

  void OnAllFaviconsLoaded(const std::vector<gfx::ImageSkia>& favicons) {
    bool needs_layout = false;
    for (const gfx::ImageSkia& favicon : favicons) {
      if (favicon.isNull()) {
        continue;
      }

      needs_layout = true;
      AddChildView(views::Builder<views::ImageView>()
                       // TODO(b/322360273): The border is temporary for more
                       // contrast until specs are ready.
                       .SetBorder(views::CreateRoundedRectBorder(
                           /*thickness=*/1,
                           /*corner_radius=*/kFaviconPreferredSize.width(),
                           SK_ColorBLACK))
                       .SetImageSize(kFaviconPreferredSize)
                       .SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
                           favicon, skia::ImageOperations::RESIZE_BEST,
                           kFaviconPreferredSize))
                       .Build());
    }

    // If at least one favicon was added, relayout.
    if (needs_layout) {
      DeprecatedLayoutImmediately();
    }
  }

  // Owned by views hierarchy.
  raw_ptr<views::ImageView> image_view_;

  base::CancelableTaskTracker cancelable_favicon_task_tracker_;

  base::WeakPtrFactory<PineItemView> weak_ptr_factory_{this};
};

BEGIN_METADATA(PineItemView)
END_METADATA

}  // namespace

PineItemsContainerView::PineItemsContainerView(
    const PineContentsData::AppsInfos& apps_infos) {
  const int elements = static_cast<int>(apps_infos.size());
  CHECK_GT(elements, 0);

  SetBackground(views::CreateRoundedRectBackground(SK_ColorWHITE,
                                                   kItemsContainerRounding));
  SetBetweenChildSpacing(pine::kItemsContainerChildSpacing);
  SetInsideBorderInsets(kItemsContainerInsets);
  SetMainAxisAlignment(views::BoxLayout::MainAxisAlignment::kCenter);
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
      AddChildView(std::make_unique<PineItemsOverflowView>(apps_infos));
      break;
    }

    std::string title;
    // `cache` might be null in a test environment. In that case, we will
    // use an empty title.
    if (cache) {
      cache->ForOneApp(
          app_info.app_id,
          [&title](const apps::AppUpdate& update) { title = update.Name(); });
    }

    // TODO(hewer|sammiequon): `PineItemView` should just take `app_info` and
    // `cache` as a constructor argument.
    PineItemView* item_view =
        AddChildView(std::make_unique<PineItemView>(title, app_info.tab_urls));

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
