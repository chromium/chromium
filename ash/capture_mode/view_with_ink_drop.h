// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_VIEW_WITH_INK_DROP_H_
#define ASH_CAPTURE_MODE_VIEW_WITH_INK_DROP_H_

#include <type_traits>

#include "ash/capture_mode/capture_mode_constants.h"
#include "base/bind.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/animation/ink_drop_impl.h"

namespace ash {

// A template class that relieves us from having to rewrite the ink drop boiler-
// plate code for all the Capture Mode views that will need it. This is used by
// CaptureModeToggleButton, CaptureModeCloseButton, ... etc.
// |T| must be a subtype of |views::InkDropHostView|.
// TODO(pbos): Move this constructor into a shared configuration method and call
// it for all uses of ViewWithInkDrop.
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
    T::ink_drop()->SetMode(views::InkDropHost::InkDropMode::ON);
    T::SetHasInkDropActionOnClick(true);
    T::ink_drop()->SetVisibleOpacity(capture_mode::kInkDropVisibleOpacity);
    views::InkDrop::UseInkDropForFloodFillRipple(T::ink_drop(),
                                                 /*highlight_on_hover=*/false,
                                                 /*highlight_on_focus=*/false);
    T::ink_drop()->SetCreateHighlightCallback(base::BindRepeating(
        [](views::InkDropHostView* host) {
          auto highlight = std::make_unique<views::InkDropHighlight>(
              gfx::SizeF(host->size()), host->ink_drop()->GetBaseColor());
          highlight->set_visible_opacity(
              capture_mode::kInkDropHighlightVisibleOpacity);
          return highlight;
        },
        this));
    // TODO(pbos): See if this is a no-op when replaced with
    // ink_drop()->SetBaseColor(), i.e. that nothing sets it later.
    T::ink_drop()->SetBaseColorCallback(
        base::BindRepeating([]() { return capture_mode::kInkDropBaseColor; }));
  }

  ~ViewWithInkDrop() override = default;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_VIEW_WITH_INK_DROP_H_
