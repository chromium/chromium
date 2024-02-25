// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/frame_sink/frame_sink_host.h"

#include "ash/frame_sink/frame_sink_holder.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "cc/trees/layer_tree_frame_sink.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

FrameSinkHost::FrameSinkHost() = default;

FrameSinkHost::~FrameSinkHost() {
  // If `host_window_` is null, it means that the `host_window_` has already
  // been destroyed and we have either destroyed or released the
  // `frame_sink_holder_`. Now we just need to release the memory of
  // this object.
  if (!host_window_) {
    return;
  }

  FrameSinkHolder::DeleteWhenLastResourceHasBeenReclaimed(
      std::move(frame_sink_holder_), host_window_);
}

void FrameSinkHost::SetPresentationCallback(PresentationCallback callback) {
  frame_sink_holder_->set_presentation_callback(std::move(callback));
}

void FrameSinkHost::Init(aura::Window* host_window) {
  InitInternal(host_window, host_window->CreateLayerTreeFrameSink());
}

void FrameSinkHost::InitForTesting(
    aura::Window* host_window,
    std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink) {
  InitInternal(host_window, std::move(layer_tree_frame_sink));
}

void FrameSinkHost::InitInternal(
    aura::Window* host_window,
    std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink) {
  SetHostWindow(host_window);
  InitFrameSinkHolder(host_window, std::move(layer_tree_frame_sink));
}

void FrameSinkHost::InitFrameSinkHolder(
    aura::Window* host_window,
    std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink) {
  DCHECK(!frame_sink_holder_) << "FrameSinkHost is already initialized.";

  frame_sink_holder_ = std::make_unique<FrameSinkHolder>(
      std::move(layer_tree_frame_sink),
      base::BindRepeating(&FrameSinkHost::CreateCompositorFrame,
                          base::Unretained(this)),
      base::BindRepeating(&FrameSinkHost::OnFirstFrameRequested,
                          base::Unretained(this)));
}

void FrameSinkHost::SetHostWindow(aura::Window* host_window) {
  DCHECK(!host_window_) << "FrameSinkHost is already initialized.";
  DCHECK(host_window);
  DCHECK(host_window->parent()) << "Before calling Init(), host_window must be "
                                   "added to the window hierarchy first.";

  host_window_ = host_window;
  host_window_observation_.Reset();
  host_window_observation_.Observe(host_window_);
}

void FrameSinkHost::UpdateSurface(const gfx::Rect& content_rect,
                                  const gfx::Rect& damage_rect,
                                  bool synchonous_draw) {
  DCHECK(frame_sink_holder_) << "Call FrameSinkHost::Init() before calling "
                                "FrameSinkHost::UpdateSurface";
  DCHECK(host_window_);

  // Turn off auto-update mode as `UpdateSurface()` should only submit one
  // frame.
  frame_sink_holder_->SetAutoUpdateMode(/*mode=*/false);

  content_rect_ = content_rect;
  UnionDamage(damage_rect);

  if (!damage_rect.IsEmpty()) {
    frame_sink_holder_->resource_manager().DamageResources();
  }

  frame_sink_holder_->SubmitCompositorFrame(synchonous_draw);
}

void FrameSinkHost::AutoUpdateSurface(const gfx::Rect& content_rect,
                                      const gfx::Rect& damage_rect) {
  DCHECK(frame_sink_holder_) << "Call FrameSinkHost::Init() before calling "
                                "FrameSinkHost::UpdateSurface";
  DCHECK(host_window_);

  content_rect_ = content_rect;
  UnionDamage(damage_rect);

  if (!damage_rect.IsEmpty()) {
    frame_sink_holder_->resource_manager().DamageResources();
  }

  frame_sink_holder_->SetAutoUpdateMode(/*mode=*/true);
}

void FrameSinkHost::OnWindowDestroying(aura::Window* window) {
  // As the `host_window_` is being destroyed, `frame_sink_holder_` should be
  // deleted as it will not be able to maintain its connection with display
  // compositor since the `LayerTreeFrameSink` gets destroyed with
  // `host_window_`.
  FrameSinkHolder::DeleteWhenLastResourceHasBeenReclaimed(
      std::move(frame_sink_holder_), host_window_);

  host_window_observation_.Reset();
  host_window_ = nullptr;
}

void FrameSinkHost::OnFirstFrameRequested() {}

}  // namespace ash
