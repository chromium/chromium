// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_VIEW_WITH_INK_DROP_H_
#define ASH_CAPTURE_MODE_VIEW_WITH_INK_DROP_H_

#include <type_traits>

#include "ash/capture_mode/capture_mode_constants.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/style/platform_style.h"

namespace ash {

// A template class that relieves us from having to rewrite the ink drop boiler-
// plate code for all the Capture Mode views that will need it. This is used by
// CaptureModeToggleButton, CaptureModeCloseButton, ... etc.
// |T| must be a subtype of |views::InkDropHostView|.
template <typename T>
class ViewWithInkDrop : public T {
 public:
  static_assert(std::is_base_of<views::InkDropHostView, T>::value,
                "T must be a subtype of views::InkDropHostView");

  // A constructor that forwards |args| to |T|'s constructor, so |args| are the
  // exact same as required by |T|'s constructor. It sets up the ink drop on the
  // view.
  template <typename... Args>
  explicit ViewWithInkDrop(Args... args) : T(std::forward<Args>(args)...) {
    T::SetInkDropMode(views::InkDropHostView::InkDropMode::ON);
    T::SetHasInkDropActionOnClick(true);
    T::SetInkDropVisibleOpacity(capture_mode::kInkDropVisibleOpacity);
  }

  ~ViewWithInkDrop() override = default;

  // views::InkDropHostView:
  std::unique_ptr<views::InkDrop> CreateInkDrop() override {
    auto ink_drop = T::CreateDefaultFloodFillInkDropImpl();
    ink_drop->SetShowHighlightOnHover(false);
    ink_drop->SetShowHighlightOnFocus(!views::PlatformStyle::kPreferFocusRings);
    return std::move(ink_drop);
  }

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override {
    auto highlight = std::make_unique<views::InkDropHighlight>(
        gfx::SizeF(T::size()), GetInkDropBaseColor());
    highlight->set_visible_opacity(
        capture_mode::kInkDropHighlightVisibleOpacity);
    return highlight;
  }

  SkColor GetInkDropBaseColor() const override {
    return capture_mode::kInkDropBaseColor;
  }
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_VIEW_WITH_INK_DROP_H_
