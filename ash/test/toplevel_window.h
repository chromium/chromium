// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_TOPLEVEL_WINDOW_H_
#define ASH_TEST_TOPLEVEL_WINDOW_H_

#include "ui/base/mojom/window_show_state.mojom-forward.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace shell {

class ToplevelWindow : public views::WidgetDelegateView {
 public:
  struct CreateParams {
    CreateParams();

    bool can_resize;
    bool can_maximize;
    bool can_fullscreen;
    bool use_saved_placement;
  };

  ToplevelWindow(const ToplevelWindow&) = delete;
  ToplevelWindow& operator=(const ToplevelWindow&) = delete;

  static views::Widget* CreateToplevelWindow(const CreateParams& params);

  // Clears saved show state and bounds used to position
  // a new window.
  static void ClearSavedStateForTest();

 private:
  explicit ToplevelWindow(const CreateParams& params);
  ~ToplevelWindow() override;

  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override;

  // views::WidgetDelegate:
  bool ShouldSaveWindowPlacement() const override;
  void SaveWindowPlacement(const gfx::Rect& bounds,
                           ui::mojom::WindowShowState show_state) override;
  bool GetSavedWindowPlacement(
      const views::Widget* widget,
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state) const override;

  bool use_saved_placement_ = true;
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_TEST_TOPLEVEL_WINDOW_H_
