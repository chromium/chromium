// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_restore/pine_item_view.h"

#include "ash/public/cpp/saved_desk_delegate.h"
#include "ash/shell.h"
#include "ash/style/typography.h"
#include "ash/wm/window_restore/pine_constants.h"
#include "base/barrier_callback.h"
#include "base/i18n/number_formatting.h"
#include "base/task/cancelable_task_tracker.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

constexpr gfx::Size kFaviconPreferredSize(16, 16);
constexpr int kItemIconBackgroundRounding = 10;
constexpr gfx::Size kItemIconPreferredSize(32, 32);
constexpr int kTitleFaviconSpacing = 4;
constexpr int kBetweenFaviconSpacing = 4;
constexpr int kTabMaxElements = 5;
constexpr int kTabOverflowThreshold = kTabMaxElements - 1;
constexpr gfx::Size kTabCountPreferredSize(24, 14);
constexpr int kTabCountRounding = 6;

}  // namespace

PineItemView::PineItemView(const std::u16string& app_title,
                           const std::vector<GURL>& favicons,
                           const size_t tab_count)
    : tab_count_(tab_count) {
  SetBetweenChildSpacing(pine::kItemChildSpacing);
  SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kCenter);
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);

  AddChildView(views::Builder<views::ImageView>()
                   .CopyAddressTo(&image_view_)
                   .SetBackground(views::CreateThemedRoundedRectBackground(
                       pine::kIconBackgroundColor, kItemIconBackgroundRounding))
                   .SetImageSize(kItemIconPreferredSize)
                   .SetPreferredSize(pine::kItemIconBackgroundPreferredSize)
                   .Build());

  // Add nested `BoxLayoutView`s, so we can have the title of the window on
  // top, and a row of favicons on the bottom.
  AddChildView(
      views::Builder<views::BoxLayoutView>()
          .SetOrientation(views::BoxLayout::Orientation::kVertical)
          .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
          .SetBetweenChildSpacing(kTitleFaviconSpacing)
          .AddChildren(
              views::Builder<views::Label>()
                  .SetEnabledColorId(pine::kPineItemTextColor)
                  .SetFontList(gfx::FontList({"Roboto"}, gfx::Font::NORMAL,
                                             pine::kItemTitleFontSize,
                                             gfx::Font::Weight::BOLD))
                  .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                  .SetText(app_title),
              views::Builder<views::BoxLayoutView>()
                  .CopyAddressTo(&favicon_container_view_)
                  .SetOrientation(views::BoxLayout::Orientation::kHorizontal)
                  .SetCrossAxisAlignment(
                      views::BoxLayout::CrossAxisAlignment::kCenter)
                  .SetBetweenChildSpacing(kBetweenFaviconSpacing))
          .Build());

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
  for (const GURL& url : favicons) {
    // TODO(b/325638530): When lacros is active, this needs to supply a valid
    // profile id.
    delegate->GetFaviconForUrl(url.spec(), /*lacros_profile_id=*/0,
                               base::BindOnce(&PineItemView::OnOneFaviconLoaded,
                                              GetWeakPtr(), barrier),
                               &cancelable_favicon_task_tracker_);
  }
}

PineItemView::~PineItemView() = default;

void PineItemView::OnOneFaviconLoaded(
    base::OnceCallback<void(const gfx::ImageSkia&)> callback,
    const gfx::ImageSkia& favicon) {
  std::move(callback).Run(favicon);
}

void PineItemView::OnAllFaviconsLoaded(
    const std::vector<gfx::ImageSkia>& favicons) {
  bool needs_layout = false;
  const size_t elements = favicons.size();
  CHECK_GE(elements, 1u);
  CHECK_LE(elements, 5u);

  for (int i = 0; i < static_cast<int>(elements); ++i) {
    // If there are overflow windows, save the last slot for a count.
    if (tab_count_ > kTabMaxElements && i >= kTabOverflowThreshold) {
      break;
    }

    const gfx::ImageSkia& favicon = favicons[i];
    // TODO(b/329454790): If favicon is null, use default icon instead.
    if (favicon.isNull()) {
      continue;
    }

    needs_layout = true;
    favicon_container_view_->AddChildView(
        views::Builder<views::ImageView>()
            // TODO(b/322360273): The border is temporary for more
            // contrast until specs are ready.
            .SetBorder(views::CreateRoundedRectBorder(
                /*thickness=*/1,
                /*corner_radius=*/kFaviconPreferredSize.width(), SK_ColorBLACK))
            .SetImageSize(kFaviconPreferredSize)
            .SetImage(gfx::ImageSkiaOperations::CreateResizedImage(
                favicon, skia::ImageOperations::RESIZE_BEST,
                kFaviconPreferredSize))
            .Build());
  }

  // Insert a count of the overflow tabs that could not be individually
  // displayed.
  if (tab_count_ > kTabMaxElements) {
    // TODO(b/329454790): Remove when default icon is added, as this should
    // already be marked true.
    needs_layout = true;

    views::Label* count_label;
    favicon_container_view_->AddChildView(
        views::Builder<views::Label>()
            .CopyAddressTo(&count_label)
            // TODO(hewer): Cut off the maximum number of digits to
            // display.
            .SetText(u"+" +
                     base::FormatNumber(tab_count_ - kTabOverflowThreshold))
            .SetPreferredSize(kTabCountPreferredSize)
            .SetEnabledColorId(cros_tokens::kCrosSysOnPrimaryContainer)
            .SetBackground(views::CreateThemedRoundedRectBackground(
                cros_tokens::kCrosSysPrimaryContainer, kTabCountRounding))
            .Build());
    TypographyProvider::Get()->StyleLabel(TypographyToken::kCrosLabel2,
                                          *count_label);
  }

  // If at least one favicon was added, relayout.
  if (needs_layout) {
    DeprecatedLayoutImmediately();
  }
}

BEGIN_METADATA(PineItemView)
END_METADATA

}  // namespace ash
