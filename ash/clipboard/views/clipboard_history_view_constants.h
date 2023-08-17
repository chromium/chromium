// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_VIEW_CONSTANTS_H_
#define ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_VIEW_CONSTANTS_H_

#include "ui/gfx/geometry/insets.h"

namespace ash::ClipboardHistoryViews {

// The insets within the contents view.
constexpr int kContentsVerticalInset = 8;
constexpr auto kContentsInsets = gfx::Insets::VH(kContentsVerticalInset, 16);

// The padding vertically separating the Ctrl+V label from the contents view.
constexpr int kCtrlVLabelPadding = 6;

// The corner radius for the footer content v2 background.
constexpr int kFooterContentV2BackgroundCornerRadius = 8;

// The child spacing for footer content v2.
constexpr int kFooterContentV2ChildSpacing = 6;

// The icon size for footer content v2.
constexpr int kFooterContentV2IconSize = 20;

// The inline icon size for footer content v2.
constexpr int kFooterContentV2InlineIconSize = 16;

// The insets for footer content v2.
constexpr auto kFooterContentV2Insets = gfx::Insets(8);

// The margins for footer content v2.
constexpr auto kFooterContentV2Margins = gfx::Insets::TLBR(8, 12, 12, 12);

// The size of the delete button.
constexpr gfx::Size kDeleteButtonSize(16, 16);

// The size of the delete button v2.
constexpr gfx::Size kDeleteButtonV2Size(20, 20);

// The size of the delete button icon.
constexpr int kDeleteButtonIconSize = 8;

// The size of the delete button v2 icon.
constexpr int kDeleteButtonV2IconSize = 20;

// The maximum number of lines allotted to a text item's label when the
// clipboard history refresh is enabled.
constexpr size_t kTextItemMaxLines = 2u;

// The margins of the `DeleteButton` instance showing on a
// `ClipboardHistoryTextItemView`.
constexpr auto kTextItemDeleteButtonMargins = gfx::Insets::TLBR(0, 8, 0, 4);

// The margins of the `DeleteButton` instance showing on
// `ClipboardHistoryBitmapItemView`.
constexpr auto kBitmapItemDeleteButtonMargins = gfx::Insets::TLBR(4, 0, 0, 4);

// The width and height of the placeholder icon for an unrendered HTML preview.
constexpr int kBitmapItemPlaceholderIconSize = 32;

// The preferred height of `ClipboardHistoryLabel`.
constexpr int kLabelPreferredHeight = 20;

// The preferred height of the image view showing on
// `ClipboardHistoryBitmapItemView`.
constexpr int kImageViewPreferredHeight = 72;

// The radius of the image view's rounded corners when offset by a background.
constexpr float kImageBackgroundCornerRadius = 12.f;

// The radius of the image view's rounded corners when surrounded by a border.
constexpr float kImageBorderCornerRadius = 4.f;

// The height of the region cut out from a contents view when the refresh is
// enabled and the menu item's delete button is showing.
constexpr float kCornerCutoutHeight = 38.f;

// The preferred size for an item's icon.
constexpr gfx::Size kIconSize(20, 20);

// The margins for an item's icon.
constexpr auto kIconMargins = gfx::Insets::TLBR(0, 0, 0, 12);

// The thickness of the image border.
constexpr int kImageBorderThickness = 1;

// TODO(http://b/267694412): Demystify this magic constant.
// The margins for inline icons.
constexpr auto kInlineIconMargins = gfx::Insets::TLBR(2, 4, 0, 0);

}  // namespace ash::ClipboardHistoryViews

#endif  // ASH_CLIPBOARD_VIEWS_CLIPBOARD_HISTORY_VIEW_CONSTANTS_H_
