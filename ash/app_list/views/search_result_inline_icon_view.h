// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_INLINE_ICON_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_INLINE_ICON_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
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
  explicit SearchResultInlineIconView(bool use_modified_styling);
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

  // Style iconified text for modifier keys like 'ctrl' and 'alt' differently.
  const bool use_modified_styling_;

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // Cached icon used to recolor icon_image_ when OnThemeChanged() is called.
  raw_ptr<const gfx::VectorIcon, ExperimentalAsh> icon_ = nullptr;

  raw_ptr<views::ImageView, ExperimentalAsh> icon_image_ =
      nullptr;  // Owned by views hierarchy.

  raw_ptr<views::Label, ExperimentalAsh> label_ =
      nullptr;  // Owned by views hierarchy.
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_INLINE_ICON_VIEW_H_
