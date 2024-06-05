// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_text_item_view.h"

#include <string>

#include "ash/bubble/bubble_utils.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/views/clipboard_history_label.h"
#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "ash/style/typography.h"
#include "base/functional/bind.h"
#include "chromeos/constants/chromeos_features.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"

namespace ash {
namespace {

// NOTE: Returns default display text elide behavior if `item` is `nullptr`.
gfx::ElideBehavior GetDisplayTextElideBehavior(
    const ClipboardHistoryItem* item) {
  constexpr auto kDefaultValue = gfx::ELIDE_TAIL;
  return item ? item->display_text_elide_behavior().value_or(kDefaultValue)
              : kDefaultValue;
}

// NOTE: Returns default display text max lines if `item` is `nullptr`.
size_t GetDisplayTextMaxLines(const ClipboardHistoryItem* item) {
  const size_t default_value =
      chromeos::features::IsClipboardHistoryRefreshEnabled()
          ? ClipboardHistoryViews::kTextItemMaxLines
          : 1u;
  return item ? item->display_text_max_lines().value_or(default_value)
              : default_value;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// TextContentsView

class ClipboardHistoryTextItemView::TextContentsView
    : public ClipboardHistoryTextItemView::ContentsView {
  METADATA_HEADER(TextContentsView, ContentsView)

 public:
  explicit TextContentsView(const ClipboardHistoryTextItemView* container) {
    const auto* item = container->GetClipboardHistoryItem();
    const auto display_text_elide_behavior = GetDisplayTextElideBehavior(item);
    const auto display_text_max_lines = GetDisplayTextMaxLines(item);

    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal));
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    AddChildView(
        views::Builder<views::BoxLayoutView>()
            .SetOrientation(views::BoxLayout::Orientation::kVertical)
            .SetCrossAxisAlignment(views::BoxLayout::CrossAxisAlignment::kStart)
            .SetProperty(views::kBoxLayoutFlexKey,
                         views::BoxLayoutFlexSpecification())
            .AddChild(views::Builder<views::Label>(
                          std::make_unique<ClipboardHistoryLabel>(
                              container->text_, display_text_elide_behavior,
                              display_text_max_lines))
                          .SetID(clipboard_history_util::kDisplayTextLabelID))
            .AfterBuild(base::BindOnce(
                [](const ClipboardHistoryItem* item,
                   views::BoxLayoutView* labels_container) {
                  if (item && item->secondary_display_text()) {
                    views::Builder<views::View>(labels_container)
                        .AddChild(views::Builder<views::Label>(
                            bubble_utils::CreateLabel(
                                TypographyToken::kCrosAnnotation2,
                                *item->secondary_display_text(),
                                cros_tokens::kCrosSysSecondary)))
                        .SetID(clipboard_history_util::
                                   kSecondaryDisplayTextLabelID)
                        .BuildChildren();
                  }
                },
                item))
            .Build());
  }

  TextContentsView(const TextContentsView& rhs) = delete;
  TextContentsView& operator=(const TextContentsView& rhs) = delete;
  ~TextContentsView() override = default;

 private:
  // ContentsView:
  SkPath GetClipPath() override {
    if (!chromeos::features::IsClipboardHistoryRefreshEnabled() ||
        !is_delete_button_visible()) {
      return SkPath();
    }

    const SkRect contents_bounds = gfx::RectToSkRect(GetContentsBounds());
    const auto width = contents_bounds.width();
    // Ensure that the clip path is tall enough for the full corner cutout to be
    // drawn. No visual problem presents if this ultimately makes the clip path
    // taller than the contents.
    const auto height = std::max(contents_bounds.height(),
                                 ClipboardHistoryViews::kCornerCutoutHeight);

    return SkPath()
        // Start at the top-left corner.
        .moveTo(0.f, 0.f)
        // Draw a vertical line to the bottom-left corner.
        .rLineTo(0.f, height)
        // Draw a horizontal line to the bottom-right corner.
        .rLineTo(width, 0.f)
        // Draw a vertical line to the start of the top-right corner's cutout.
        .lineTo(width, ClipboardHistoryViews::kCornerCutoutHeight)
        // Draw the top-right corner's cutout.
        .rCubicTo(0.f, -8.f, -6.7f, -10.f, -10.f, -10.f)
        .rLineTo(-4.f, 0.f)
        .rCubicTo(-7.7f, 0.f, -14.f, -6.3f, -14.f, -14.f)
        .rLineTo(0.f, -4.f)
        .rCubicTo(0.f, -3.3f, -2.f, -10.f, -10.f, -10.f)
        // Draw a horizontal line back to the starting point.
        .lineTo(0.f, 0.f)
        .close();
  }
};

BEGIN_METADATA(ClipboardHistoryTextItemView, TextContentsView)
END_METADATA

////////////////////////////////////////////////////////////////////////////////
// ClipboardHistoryTextItemView

ClipboardHistoryTextItemView::ClipboardHistoryTextItemView(
    const base::UnguessableToken& item_id,
    const ClipboardHistory* clipboard_history,
    views::MenuItemView* container)
    : ClipboardHistoryItemView(item_id, clipboard_history, container),
      text_(GetClipboardHistoryItem()->display_text()) {
  GetViewAccessibility().SetName(text_);
}

ClipboardHistoryTextItemView::~ClipboardHistoryTextItemView() = default;

std::unique_ptr<ClipboardHistoryTextItemView::ContentsView>
ClipboardHistoryTextItemView::CreateContentsView() {
  return std::make_unique<TextContentsView>(this);
}

BEGIN_METADATA(ClipboardHistoryTextItemView)
END_METADATA

}  // namespace ash
