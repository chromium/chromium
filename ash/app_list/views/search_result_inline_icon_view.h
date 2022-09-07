// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_INLINE_ICON_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_INLINE_ICON_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
class Label;
}  // namespace views

namespace ash {

// Displays a rounded rect bubble containing styled text xor a vector icon.
class ASH_EXPORT SearchResultInlineIconView : public views::View {
 public:
  SearchResultInlineIconView();
  SearchResultInlineIconView(const SearchResultInlineIconView&) = delete;
  SearchResultInlineIconView& operator=(const SearchResultInlineIconView&) =
      delete;
  ~SearchResultInlineIconView() override;

  // Setup the `SearchResultInlineIconView` to show an icon or iconified text.
  // Showing both in the same view is not supported.
  void SetIcon(const gfx::VectorIcon& icon);
  void SetText(const std::u16string& text);

 private:
  class SizedLabel;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

  // Cached icon used to recolor icon_image_ when OnThemeChanged() is called.
  const gfx::VectorIcon* icon_ = nullptr;

  views::ImageView* icon_image_ = nullptr;  // Owned by views hierarchy.

  views::Label* label_ = nullptr;  // Owned by views hierarchy.
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_INLINE_ICON_VIEW_H_
