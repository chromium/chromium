// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_item_view.h"

#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/style/typography.h"
#include "ash/wm/window_restore/pine_app_image_view.h"
#include "ash/wm/window_restore/pine_constants.h"
#include "ash/wm/window_restore/window_restore_util.h"
#include "base/barrier_callback.h"
#include "base/i18n/number_formatting.h"
#include "base/task/cancelable_task_tracker.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/highlight_border.h"

namespace ash {

namespace {

constexpr gfx::Size kFaviconPreferredSize(16, 16);
constexpr int kTitleFaviconSpacing = 4;
constexpr int kBetweenFaviconSpacing = 4;
constexpr int kTabMaxElements = 5;
constexpr int kTabOverflowThreshold = kTabMaxElements - 1;
constexpr gfx::Size kTabCountPreferredSize(24, 14);
constexpr int kTabCountRounding = 6;

}  // namespace

PineItemView::PineItemView(const PineContentsData::AppInfo& app_info,
                           bool inside_screenshot)
    : app_id_(app_info.app_id),
      tab_count_(app_info.tab_count),
      inside_screenshot_(inside_screenshot) {
  SetBetweenChildSpacing(inside_screenshot_
                             ? pine::kScreenshotIconRowChildSpacing
                             : pine::kItemChildSpacing);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetID(pine::kItemViewID);

  auto* image_view = AddChildView(std::make_unique<PineAppImageView>(
      app_id_,
      inside_screenshot_ ? PineAppImageView::Type::kScreenshot
                         : PineAppImageView::Type::kItem,
      base::BindOnce(&PineItemView::UpdateTitle,
                     weak_ptr_factory_.GetWeakPtr())));
  image_view->SetID(pine::kItemImageViewID);

  if (inside_screenshot_) {
    views::Separator* separator =
        AddChildView(std::make_unique<views::Separator>());
    separator->SetColorId(ui::kColorAshSystemUIMenuSeparator);
    separator->SetPreferredLength(pine::kScreenshotIconRowIconSize);
  }

  // Add nested `BoxLayoutView`s, so we can have the title of the window on
  // top, and a row of favicons on the bottom.
  if (inside_screenshot_) {
    AddChildView(
        views::Builder<views::BoxLayoutView>()
            .CopyAddressTo(&favicon_container_view_)
            .SetID(pine::kFaviconContainerViewID)
            .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
            .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
            .SetBetweenChildSpacing(pine::kScreenshotFaviconSpacing)
            .Build());
  } else {
    AddChildView(
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kVertical)
            .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
            .SetBetweenChildSpacing(kTitleFaviconSpacing)
            .AddChildren(
                views::Builder<views::Label>()
                    .CopyAddressTo(&title_label_view_)
                    .SetEnabledColorId(pine::kPineItemTextColorId)
                    .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                    .CustomConfigure(base::BindOnce(
                        [](const base::WeakPtr<PineItemView> weak_this,
                           views::Label* label) {
                          TypographyProvider::Get()->StyleLabel(
                              TypographyToken::kCrosButton2, *label);
                          if (weak_this) {
                            weak_this->UpdateTitle();
                          }
                        },
                        weak_ptr_factory_.GetWeakPtr())),
                views::Builder<views::BoxLayoutView>()
                    .CopyAddressTo(&favicon_container_view_)
                    .SetID(pine::kFaviconContainerViewID)
                    .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
                    .SetCrossAxisAlignment(
                        views::BoxLayout::CrossAxisAlignment::kCenter)
                    .SetBetweenChildSpacing(kBetweenFaviconSpacing))
            .Build());
  }

  const std::vector<GURL>& favicons = app_info.tab_urls;
  if (favicons.empty()) {
    return;
  }

  // Use a barrier callback so that we only layout once after all favicons are
  // added as views. Pair each `gfx::ImageSkia` with its index so we can
  // restore the order after all icons have loaded, as `BarrierCallback` is not
  // guaranteed to maintain the order that the callbacks are initially run.
  auto barrier = base::BarrierCallback<const IndexedImagePair&>(
      /*num_callbacks=*/favicons.size(),
      /*done_callback=*/base::BindOnce(&PineItemView::OnAllFaviconsLoaded,
                                       weak_ptr_factory_.GetWeakPtr()));

  auto* delegate = Shell::Get()->saved_desk_delegate();
  for (int i = 0; i < static_cast<int>(favicons.size()); ++i) {
    const GURL& url = favicons[i];
    delegate->GetFaviconForUrl(url.spec(), app_info.lacros_profile_id,
                               base::BindOnce(&PineItemView::OnOneFaviconLoaded,
                                              GetWeakPtr(), barrier, i),
                               &cancelable_favicon_task_tracker_);
  }
}

PineItemView::~PineItemView() = default;

void PineItemView::OnOneFaviconLoaded(IndexedImageCallback callback,
                                      int index,
                                      const gfx::ImageSkia& favicon) {
  std::move(callback).Run({index, favicon});
}

void PineItemView::OnAllFaviconsLoaded(
    std::vector<IndexedImagePair> indexed_favicons) {
  base::ranges::sort(indexed_favicons,
                     [](const auto& element_a, const auto& element_b) {
                       return element_a.first < element_b.first;
                     });

  bool needs_layout = false;
  const size_t elements = indexed_favicons.size();
  CHECK_GE(elements, 1u);
  CHECK_LE(elements, 5u);

  int count = 0;
  for (const auto& [_, favicon] : indexed_favicons) {
    // If there are overflow windows, save the last slot for a count.
    if (tab_count_ > kTabMaxElements && count >= kTabOverflowThreshold) {
      break;
    }

    needs_layout = true;

    views::Builder<views::ImageView> builder;
    builder
        .SetBorder(std::make_unique<views::HighlightBorder>(
            /*corner_radius=*/kFaviconPreferredSize.width(),
            views::HighlightBorder::Type::kHighlightBorderNoShadow))
        .SetImageSize(kFaviconPreferredSize);

    if (inside_screenshot_) {
      builder.SetPreferredSize(pine::kScreenshotIconRowImageViewSize);
    }

    // If the image data is null, use a default cube icon instead.
    if (favicon.isNull()) {
      builder
          .SetImage(ui::ImageModel::FromVectorIcon(
              kDefaultAppIcon, cros_tokens::kCrosSysOnPrimary))
          .SetBackground(views::CreateThemedRoundedRectBackground(
              cros_tokens::kCrosSysPrimary, kFaviconPreferredSize.width()));
    } else {
      builder.SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
          favicon, skia::ImageOperations::RESIZE_BEST, kFaviconPreferredSize));
    }

    favicon_container_view_->AddChildView(std::move(builder).Build());
    ++count;
  }

  // Insert a count of the overflow tabs that could not be individually
  // displayed.
  if (tab_count_ > kTabMaxElements) {
    views::Label* count_label;
    favicon_container_view_->AddChildView(
        views::Builder<views::Label>()
            .CopyAddressTo(&count_label)
            // TODO(hewer): Cut off the maximum number of digits to
            // display.
            .SetText(u"+" +
                     base::FormatNumber(tab_count_ - kTabOverflowThreshold))
            .SetPreferredSize(inside_screenshot_
                                  ? pine::kScreenshotIconRowImageViewSize
                                  : kTabCountPreferredSize)
            .SetEnabledColorId(cros_tokens::kCrosSysOnPrimaryContainer)
            .SetBackground(views::CreateThemedRoundedRectBackground(
                cros_tokens::kCrosSysPrimaryContainer,
                inside_screenshot_ ? pine::kScreenshotIconRowIconSize / 2
                                   : kTabCountRounding))
            .Build());
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosLabel2,
                                          *count_label);
  }

  // If at least one favicon was added, relayout.
  if (needs_layout) {
    DeprecatedLayoutImmediately();
  }
}

void PineItemView::UpdateTitle() {
  apps::AppRegistryCache* cache =
      apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(
          Shell::Get()->session_controller()->GetActiveAccountId());

  // `title` will be the window title from the previous session stored in the
  // full restore file. The title fetched from the app service would more
  // accurate, but the app might not be installed yet. Browsers are always
  // installed and `title` will be the active tab title fetched from session
  // restore. `cache` might be null in a test environment.
  std::string title;
  if (cache && (title.empty() || !IsBrowserAppId(app_id_))) {
    cache->ForOneApp(app_id_, [&title](const apps::AppUpdate& update) {
      title = update.Name();
    });
  }

  title_label_view_->SetText(base::UTF8ToUTF16(title));
}

BEGIN_METADATA(PineItemView)
END_METADATA

}  // namespace ash
