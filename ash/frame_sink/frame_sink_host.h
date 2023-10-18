// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_SINK_FRAME_SINK_HOST_H_
#define ASH_FRAME_SINK_FRAME_SINK_HOST_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/frame_sink/ui_resource_manager.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/presentation_feedback.h"

namespace cc {
class LayerTreeFrameSink;
}  // namespace cc

namespace ash {

class FrameSinkHolder;

// Base class for surfaces that render content by creating independent
// compositor frames and submitting them directly to display compositor.
//
// FrameSinkHost encapsulates interactions with the display compositor and
// manages relevant resources. Child classes override CreateCompositorFrame() to
// control compositor frame creation behavior.
//
// Note: The host_window must outlive the FrameSinkHost.
class ASH_EXPORT FrameSinkHost : public aura::WindowObserver {
 public:
  using PresentationCallback =
      base::RepeatingCallback<void(const gfx::PresentationFeedback&)>;

  FrameSinkHost();

  FrameSinkHost(const FrameSinkHost&) = delete;
  FrameSinkHost& operator=(const FrameSinkHost&) = delete;

  ~FrameSinkHost() override;

  aura::Window* host_window() { return host_window_; }
  const aura::Window* host_window() const { return host_window_; }

  void SetPresentationCallback(PresentationCallback callback);

  // Initializes the FrameSinkHost on the host_window.
  virtual void Init(aura::Window* host_window);

  virtual void InitForTesting(
      aura::Window* host_window,
      std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink);

  // Updates the surface by submitting a compositor frame. With
  // synchronous_draw as true, we send a compositor frame as soon as we call the
  // UpdateSurface method otherwise we draw asynchronously where we wait till
  // display_compositor requests for a new frame.
  // Note: Calling this method turns off the auto updating of the surface if
  // enabled.
  void UpdateSurface(const gfx::Rect& content_rect,
                     const gfx::Rect& damage_rect,
                     bool synchronous_draw);

  // Updates the surface by submitting frames asynchronously. Compared to
  // `UpdateSurface()`, where we submit a single frame, after calling
  // this method, we keep submitting frames asynchronously. It should be used if
  // we expect that there are continuous updates within the content_rect.
  void AutoUpdateSurface(const gfx::Rect& content_rect,
                         const gfx::Rect& damage_rect);

  // Overridden from aura::WindowObserver
  void OnWindowDestroying(aura::Window* window) override;

 protected:
  // Creates a compositor frame that can be sent to the display compositor.
  // `begin_frame_ack` is a token that needs to be attached to the compositor
  // frame being created.
  // `resource_manager` helps manage resources that can be attached to the
  // compositor frame and also give us a pool of reusable resources.
  // `auto_update` if true means that we are continuously submitting frames
  // asynchronously and should redraw full surface regardless of damage.
  // `last_submitted_frame_size` and `last_submitted_frame_dsf`
  // can be used to determine if a new surface needs to be identified on the
  // `host_window_`.
  // Returns nullptr if a compositor frame cannot be created.
  virtual std::unique_ptr<viz::CompositorFrame> CreateCompositorFrame(
      const viz::BeginFrameAck& begin_frame_ack,
      UiResourceManager& resource_manager,
      bool auto_update,
      const gfx::Size& last_submitted_frame_size,
      float last_submitted_frame_dsf) = 0;

  // Callback invoked when underlying frame sink holder gets the first begin
  // frame from viz. This signifies that the gpu process has been fully
  // initialized.
  virtual void OnFirstFrameRequested();

  const gfx::Rect& GetTotalDamage() const { return total_damage_rect_; }

  void UnionDamage(const gfx::Rect& rect) { total_damage_rect_.Union(rect); }

  void IntersectDamage(const gfx::Rect& rect) {
    total_damage_rect_.Intersect(rect);
  }

  void ResetDamage() { total_damage_rect_ = gfx::Rect(); }

  const gfx::Rect& GetContentRect() const { return content_rect_; }

 private:
  void InitFrameSinkHolder(
      aura::Window* host_window,
      std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink);

  void SetHostWindow(aura::Window* host_window);

  void InitInternal(
      aura::Window* host_window,
      std::unique_ptr<cc::LayerTreeFrameSink> layer_tree_frame_sink);

  // Observation to track the lifetime of `host_window_`.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      host_window_observation_{this};

  // The window on which LayerTreeFrameSink is created on.
  raw_ptr<aura::Window> host_window_ = nullptr;

  // The bounds of the content to be displayed in host window coordinates.
  gfx::Rect content_rect_;

  // The damage rect in host window coordinates.
  gfx::Rect total_damage_rect_;

  // Holds the LayerTreeFrameSink. For proper deletion of in flight
  // resources, lifetime of the FrameSinkHolder is extended to either the root
  // window of the`host_window` or till we reclaim all the exported resources
  // to the display compositor. See `FrameSinkHolder` implementation for
  // details. https://crbug.com/765763
  std::unique_ptr<FrameSinkHolder> frame_sink_holder_;

  base::WeakPtrFactory<FrameSinkHost> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_FRAME_SINK_FRAME_SINK_HOST_H_
