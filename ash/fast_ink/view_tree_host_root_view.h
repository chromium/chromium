// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_VIEW_TREE_HOST_ROOT_VIEW_H_
#define ASH_FAST_INK_VIEW_TREE_HOST_ROOT_VIEW_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/frame_sink/frame_sink_host.h"
#include "base/memory/weak_ptr.h"
#include "ui/views/widget/root_view.h"

namespace views {
class Widget;
}  // namespace views

namespace viz {
class CompositorFrame;
}  // namespace viz

namespace gfx {
class Size;
class Rect;
}  // namespace gfx

namespace ash {

class ViewTreeHostRootViewFrameFactory;

// ViewTreeHostRootView is a view that submits a compositor frame directly.
// TODO(oshima): Support partial content update & front buffer rendering and
// replace FastInkView.
class ASH_EXPORT ViewTreeHostRootView : public views::internal::RootView,
                                        public ash::FrameSinkHost {
 public:
  explicit ViewTreeHostRootView(views::Widget* widget);

  ViewTreeHostRootView(const ViewTreeHostRootView&) = delete;
  ViewTreeHostRootView& operator=(const ViewTreeHostRootView&) = delete;

  ~ViewTreeHostRootView() override;

  void SchedulePaintInRect(const gfx::Rect& damage_rect);

  bool GetIsOverlayCandidate();
  void SetIsOverlayCandidate(bool is_overlay_candidate);

  // ash::FrameSinkHost:
  std::unique_ptr<viz::CompositorFrame> CreateCompositorFrame(
      const viz::BeginFrameAck& begin_frame_ack,
      UiResourceManager& resource_manager,
      bool auto_update,
      const gfx::Size& last_submitted_frame_size,
      float last_submitted_frame_dsf) override;

 private:
  // True if the content needs to use hardware-overlays.
  bool is_overlay_candidate_ = true;

  std::unique_ptr<ViewTreeHostRootViewFrameFactory> frame_factory_;

  base::WeakPtrFactory<ViewTreeHostRootView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_FAST_INK_VIEW_TREE_HOST_ROOT_VIEW_H_
