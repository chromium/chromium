// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_COMPAT_MODE_OVERLAY_DIALOG_H_
#define ASH_COMPONENTS_ARC_COMPAT_MODE_OVERLAY_DIALOG_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "ui/views/layout/flex_layout_view.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class View;
}  // namespace views

namespace arc {

class OverlayDialog : public views::FlexLayoutView {
  METADATA_HEADER(OverlayDialog, views::FlexLayoutView)

 public:
  OverlayDialog(const OverlayDialog&) = delete;
  OverlayDialog& operator=(const OverlayDialog&) = delete;
  ~OverlayDialog() override;

  // Show |dialog_view| on |base_window| with "scrim" (semi-transparent black)
  // background and horizontal margin. If |dialog_view| is nullptr, only the
  // background is shown.
  // The |dialog_view|'s width is responsive to the width of |base_window|. It
  // matches the |base_window|'s width inside the horizontal margin unless it
  // exceeds the |dialog_view|'s preferred width. Note that if |base_window| has
  // another overlay already, the existing overlay is closed.
  static void Show(aura::Window* base_window,
                   base::OnceClosure on_destroying,
                   std::unique_ptr<views::View> dialog_view);

  // Close overlay view on |base_window| if it has any.
  static void CloseIfAny(aura::Window* base_window);

  // views::View:
  void AddedToWidget() override;
  void OnThemeChanged() override;

 private:
  friend class OverlayDialogTest;

  OverlayDialog(base::OnceClosure on_destroying,
                std::unique_ptr<views::View> dialog_view);

  const bool has_dialog_view_;

  base::ScopedClosureRunner scoped_callback_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_COMPAT_MODE_OVERLAY_DIALOG_H_
