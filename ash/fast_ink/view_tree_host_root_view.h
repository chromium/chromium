// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FAST_INK_VIEW_TREE_HOST_ROOT_VIEW_H_
#define ASH_FAST_INK_VIEW_TREE_HOST_ROOT_VIEW_H_

#include <memory>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/viz/common/quads/compositor_frame_metadata.h"
#include "components/viz/common/resources/resource_id.h"
#include "gpu/command_buffer/common/mailbox.h"
#include "ui/views/widget/root_view.h"

namespace gfx {
struct PresentationFeedback;
}

namespace views {
class Widget;
}

namespace ash {

// ViewTreeHostRootView is a view that submits a compositor frame directly.
// TODO(oshima): Support partial content update & front buffer rendering and
// replace FastInkView.
class ViewTreeHostRootView : public views::internal::RootView {
 public:
  using PresentationCallback =
      base::RepeatingCallback<void(const gfx::PresentationFeedback&)>;

  explicit ViewTreeHostRootView(views::Widget* widget);

  ViewTreeHostRootView(const ViewTreeHostRootView&) = delete;
  ViewTreeHostRootView& operator=(const ViewTreeHostRootView&) = delete;

  ~ViewTreeHostRootView() override;

  // Set presentation callback.
  void set_presentation_callback(PresentationCallback callback) {
    presentation_callback_ = std::move(callback);
  }

  void SchedulePaintInRect(const gfx::Rect& rect);

  bool GetIsOverlayCandidate();
  void SetIsOverlayCandidate(bool is_overlay_candidate);

 private:
  struct Resource;
  class LayerTreeViewTreeFrameSinkHolder;

  std::unique_ptr<Resource> ObtainResource();
  void Paint();

  // Update content with the |resource| and damage rectangles for surface.
  void UpdateSurface(const gfx::Rect& damage_rect,
                     std::unique_ptr<Resource> resource);

  void SubmitCompositorFrame();
  void SubmitPendingCompositorFrame();
  void ReclaimResource(std::unique_ptr<Resource> resource);
  void DidReceiveCompositorFrameAck();
  void DidPresentCompositorFrame(const gfx::PresentationFeedback& feedback);

  // Constants initialized in constructor.
  PresentationCallback presentation_callback_;

  // The rotation tranfrom from the panel's original rotation to
  // the current logical rotation.
  gfx::Transform rotate_transform_;

  // GPU Memory buffer size.
  gfx::Size buffer_size_;

  gfx::Rect damaged_paint_rect_;
  bool pending_paint_ = false;

  // overlay candidate in submitted frame data.
  bool is_overlay_candidate_ = true;

  // The resource to be submitted.
  std::unique_ptr<Resource> pending_resource_;

  int resource_group_id_ = 1;
  viz::ResourceIdGenerator id_generator_;
  // Total damaged rect in surface.
  gfx::Rect damage_rect_;
  bool pending_compositor_frame_ack_ = false;
  viz::FrameTokenGenerator next_frame_token_;
  std::vector<std::unique_ptr<Resource>> returned_resources_;
  std::unique_ptr<LayerTreeViewTreeFrameSinkHolder> frame_sink_holder_;
  base::WeakPtrFactory<ViewTreeHostRootView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_FAST_INK_VIEW_TREE_HOST_ROOT_VIEW_H_
