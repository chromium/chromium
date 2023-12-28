// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_INLINE_ICON_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_INLINE_ICON_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

// Displays a rounded rect bubble containing styled text xor a vector icon.
// Setting `use_modified_styling` to `true` changes the style of iconified text
// for modifier keys like 'ctrl' and 'alt'. Setting `is_first_key` to `true`
// changes the left margin of the view to 0 so it can be left-aligned with other
// elements.
class ASH_EXPORT SearchResultInlineIconView : public views::View {
  METADATA_HEADER(SearchResultInlineIconView, views::View)

 public:
  SearchResultInlineIconView(bool use_modified_styling,
                             bool is_first_key = false);
  SearchResultInlineIconView(const SearchResultInlineIconView&) = delete;
  SearchResultInlineIconView& operator=(const SearchResultInlineIconView&) =
      delete;
  ~SearchResultInlineIconView() override;

  // Setup the `SearchResultInlineIconView` to show an icon or iconified text.
  // Showing both in the same view is not supported.
  void SetIcon(const gfx::VectorIcon& icon);
  void SetText(const std::u16string& text);

  // Sets the tooltip text on `icon_image_`.
  void SetTooltipTextForImageView(const std::u16string& text);

 private:
  class SizedLabel;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // Style iconified text for modifier keys like 'ctrl' and 'alt' differently.
  const bool use_modified_styling_;

  // Cached icon used to recolor icon_image_ when OnThemeChanged() is called.
  raw_ptr<const gfx::VectorIcon> icon_ = nullptr;

  raw_ptr<views::ImageView> icon_image_ = nullptr;  // Owned by views hierarchy.

  raw_ptr<views::Label> label_ = nullptr;  // Owned by views hierarchy.
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_INLINE_ICON_VIEW_H_
