// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_FAST_INK_VIEW_H_
#define ASH_FAST_INK_FAST_INK_VIEW_H_

#include <memory>
#include <vector>

#include "ash/fast_ink/fast_ink_host.h"
#include "ui/aura/window.h"
#include "ui/gfx/canvas.h"
#include "ui/views/view.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace gfx {
class GpuMemoryBuffer;
}  // namespace gfx

namespace fast_ink {

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
  // FastInkHost::UpdateSurface for more detials.
  void UpdateSurface(const gfx::Rect& content_rect,
                     const gfx::Rect& damage_rect,
                     bool auto_refresh);

  virtual FastInkHost::PresentationCallback GetPresentationCallback();

 protected:
  // Helper class that provides flicker free painting to a GPU memory buffer.
  class ScopedPaint {
   public:
    ScopedPaint(FastInkView* view, const gfx::Rect& damage_rect_in_window);

    ScopedPaint(const ScopedPaint&) = delete;
    ScopedPaint& operator=(const ScopedPaint&) = delete;

    ~ScopedPaint();

    gfx::Canvas& canvas() { return canvas_; }

   private:
    gfx::GpuMemoryBuffer* const gpu_memory_buffer_;
    // Damage rect in the buffer coordinates.
    const gfx::Rect damage_rect_;
    gfx::Canvas canvas_;
  };

  FastInkView();

  void SetFastInkHost(std::unique_ptr<FastInkHost> host);
  FastInkHost* host() { return host_.get(); }

 private:
  std::unique_ptr<FastInkHost> host_;
};

}  // namespace fast_ink

#endif  // ASH_FAST_INK_FAST_INK_VIEW_H_
