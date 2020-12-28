// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_VIEW_CONSTANTS_H_
#define ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_VIEW_CONSTANTS_H_

#include "ui/gfx/geometry/insets.h"

namespace ash {
namespace ClipboardHistoryViews {

// The insets within the contents view.
constexpr int kContentsVerticalInset = 8;
constexpr gfx::Insets kContentsInsets(kContentsVerticalInset,
                                      /*horizontal=*/16);

// The size of the `DeleteButton`.
constexpr int kDeleteButtonSizeDip = 16;

// The margins of the `DeleteButton` instance showing on
// `ClipboardHistoryTextItemView` or `ClipboardHistoryFileItemView`.
constexpr gfx::Insets kDefaultItemDeleteButtonMargins =
    gfx::Insets(/*top=*/0, /*left=*/8, /*bottom=*/0, /*right=*/4);

// The margins of the `DeleteButton` instance showing on
// `ClipboardHistoryBitmapItemView`.
constexpr gfx::Insets kBitmapItemDeleteButtonMargins =
    gfx::Insets(/*top=*/4, /*left=*/0, /*bottom=*/0, /*right=*/4);

// The preferred height of `ClipboardHistoryLabel`.
constexpr int kLabelPreferredHeight = 20;

// The preferred height of the image view showing on
// `ClipboardHistoryBitmapItemView`.
constexpr int kImageViewPreferredHeight = 72;

// The radius of the image's rounded corners.
constexpr int kImageRoundedCornerRadius = 4;

// The thickness of the image border.
constexpr int kImageBorderThickness = 1;

}  // namespace ClipboardHistoryViews
}  // namespace ash

#endif  // ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_VIEW_CONSTANTS_H_
