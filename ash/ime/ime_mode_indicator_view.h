// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IME_IME_MODE_INDICATOR_VIEW_H_
#define ASH_IME_IME_MODE_INDICATOR_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/widget/widget.h"

namespace views {
class Label;
}  // namespace views

namespace ash {

// A small bubble that shows the short name of the current IME (e.g. "DV" for
// Dvorak) after switching IMEs with an accelerator (e.g. Ctrl-Space).
class ASH_EXPORT ImeModeIndicatorView : public views::BubbleDialogDelegateView {
  METADATA_HEADER(ImeModeIndicatorView, views::BubbleDialogDelegateView)

 public:
  // The cursor bounds is in the universal screen coordinates in DIP.
  ImeModeIndicatorView(const gfx::Rect& cursor_bounds,
                       const std::u16string& label);
  ImeModeIndicatorView(const ImeModeIndicatorView&) = delete;
  ImeModeIndicatorView& operator=(const ImeModeIndicatorView&) = delete;
  ~ImeModeIndicatorView() override;

  // Show the mode indicator then hide with fading animation.
  void ShowAndFadeOut();

  // views::BubbleDialogDelegateView override:
  void OnBeforeBubbleWidgetInit(views::Widget::InitParams* params,
                                views::Widget* widget) const override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Init() override;

 protected:
  // views::WidgetDelegateView overrides:
  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override;

 private:
  gfx::Rect cursor_bounds_;
  raw_ptr<views::Label> label_view_;
  base::OneShotTimer timer_;
};

}  // namespace ash

#endif  // ASH_IME_IME_MODE_INDICATOR_VIEW_H_
