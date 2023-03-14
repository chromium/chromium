// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/view_tree_host_root_view.h"

#include <memory>

#include "ash/fast_ink/view_tree_host_root_view_frame_factory.h"
#include "ash/frame_sink/frame_sink_host.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/widget/widget.h"

namespace ash {

ViewTreeHostRootView::ViewTreeHostRootView(views::Widget* widget)
    : views::internal::RootView(widget),
      frame_factory_(
          std::make_unique<ViewTreeHostRootViewFrameFactory>(widget)) {}

ViewTreeHostRootView::~ViewTreeHostRootView() = default;

bool ViewTreeHostRootView::GetIsOverlayCandidate() {
  return is_overlay_candidate_;
}

void ViewTreeHostRootView::SetIsOverlayCandidate(bool is_overlay_candidate) {
  is_overlay_candidate_ = is_overlay_candidate;
}

void ViewTreeHostRootView::SchedulePaintInRect(const gfx::Rect& damage_rect) {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&ViewTreeHostRootView::UpdateSurface,
                                weak_ptr_factory_.GetWeakPtr(),
                                /*content_rect=*/gfx::Rect(size()), damage_rect,
                                /*synchonous_draw=*/false));
}

std::unique_ptr<viz::CompositorFrame>
ViewTreeHostRootView::CreateCompositorFrame(
    const viz::BeginFrameAck& begin_frame_ack,
    UiResourceManager& resource_manager,
    bool auto_update,
    const gfx::Size& last_submitted_frame_size,
    float last_submitted_frame_dsf) {
  TRACE_EVENT1("ui", "ViewTreeHostRootView::SubmitCompositorFrame", "damage",
               GetTotalDamage().ToString());

  auto frame = frame_factory_->CreateCompositorFrame(
      begin_frame_ack, GetContentRect(), GetTotalDamage(),
      is_overlay_candidate_, resource_manager);

  ResetDamage();

  // A change in the size of the compositor frame means we need to identify a
  // new surface to submit the compositor frame to since the surface size is now
  // different.
  if (last_submitted_frame_size != frame->size_in_pixels() ||
      last_submitted_frame_dsf != frame->device_scale_factor()) {
    host_window()->AllocateLocalSurfaceId();
  }

  return frame;
}

}  // namespace ash
