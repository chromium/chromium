// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_FAST_INK_VIEW_H_
#define ASH_FAST_INK_FAST_INK_VIEW_H_

#include <memory>
#include <vector>

#include "ash/fast_ink/fast_ink_host.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace ash {

// FastInkView is a view supporting low-latency rendering by using FastInkHost.
// The view's widget must have the same bounds as a root window (covers the
// entire display). FastInkHost for more details.
class FastInkView : public views::View {
 public:
  ~FastInkView() override;
  FastInkView(const FastInkView&) = delete;
  FastInkView& operator=(const FastInkView&) = delete;

  // Function to create a container Widget, pass ownership of |fast_ink_view|
  // as the contents view to the Widget. fast_ink_view fills the bounds of the
  // root_window.
  static views::UniqueWidgetPtr CreateWidgetWithContents(
      std::unique_ptr<FastInkView> fast_ink_view,
      aura::Window* container);

  // Update content and damage rectangles for surface. See
  // FastInkHost::UpdateSurface for more details.
  void UpdateSurface(const gfx::Rect& content_rect,
                     const gfx::Rect& damage_rect,
                     bool auto_refresh);

  // Gets a handle that paints to the GPU buffer that is associated with the
  // FastInk surface without flickers.
  std::unique_ptr<FastInkHost::ScopedPaint> GetScopedPaint(
      const gfx::Rect& damage_rect_in_window) const;

 protected:
  FastInkView();

  FastInkHost* host() { return host_.get(); }
  void SetFastInkHost(std::unique_ptr<FastInkHost> host);

  virtual FastInkHost::PresentationCallback GetPresentationCallback();

 private:
  std::unique_ptr<FastInkHost> host_;
};

}  // namespace ash

#endif  // ASH_FAST_INK_FAST_INK_VIEW_H_
