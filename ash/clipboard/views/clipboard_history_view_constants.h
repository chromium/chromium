// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_VIEW_CONSTANTS_H_
#define ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_VIEW_CONSTANTS_H_

#include "ui/gfx/geometry/insets.h"

namespace ash {
namespace ClipboardHistoryViews {

// The insets within the contents view.
constexpr int kContentsVerticalInset = 8;
constexpr auto kContentsInsets = gfx::Insets::VH(kContentsVerticalInset, 16);

// The size of the `DeleteButton`.
constexpr int kDeleteButtonSizeDip = 16;

// The margins of the `DeleteButton` instance showing on
// `ClipboardHistoryTextItemView` or `ClipboardHistoryFileItemView`.
constexpr auto kDefaultItemDeleteButtonMargins = gfx::Insets::TLBR(0, 8, 0, 4);

// The margins of the `DeleteButton` instance showing on
// `ClipboardHistoryBitmapItemView`.
constexpr auto kBitmapItemDeleteButtonMargins = gfx::Insets::TLBR(4, 0, 0, 4);

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
