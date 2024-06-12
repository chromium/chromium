// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_FRAME_SINK_FRAME_SINK_HOLDER_H_
#define ASH_FRAME_SINK_FRAME_SINK_HOLDER_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/frame_sink/frame_sink_host.h"
#include "ash/frame_sink/ui_resource_manager.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "cc/scheduler/scheduler.h"
#include "cc/trees/layer_tree_frame_sink_client.h"
#include "components/viz/common/frame_sinks/begin_frame_source.h"
#include "components/viz/common/quads/compositor_frame.h"
#include "ui/aura/window_observer.h"

namespace cc {
class LayerTreeFrameSink;
}  // namespace cc

namespace ash {

class FrameSinkHolderTestApi;

// Holds the LayerTreeFrameSink and manages all the interactions with the
// LayerTreeFrameSink. It provides an API to submit compositor frames either
// synchronously or asynchronously. We have this holder class so that, if
// needed, we can make LayerTreeFrameSink outlive the frame_sink_host in order
// to reclaim any exported resources to display compositor. Note: The class is
// intended to be used by the FrameSinkHost class.
class ASH_EXPORT FrameSinkHolder final : public cc::LayerTreeFrameSinkClient,
                                         public viz::BeginFrameObserverBase,
                                         public aura::WindowObserver {
 public:
  using PresentationCallback =
      base::RepeatingCallback<void(const gfx::PresentationFeedback&)>;

  // Refer to declaration of `FrameSinkHost::CreateCompositorFrame` for a
  // detailed comment.
  using GetCompositorFrameCallback =
      base::RepeatingCallback<std::unique_ptr<viz::CompositorFrame>(
          const viz::BeginFrameAck& begin_frame_ack,
          UiResourceManager& resource_manager,
          bool auto_update,
          const gfx::Size& last_submitted_frame_size,
          float last_submitted_frame_dsf)>;
  // Refer to declaration of `FrameSinkHost::OnFirstFrameRequested` for a
  // detailed comment.
  using OnFirstFrameRequestedCallback = base::RepeatingCallback<void()>;

  FrameSinkHolder(
      std::unique_ptr<cc::LayerTreeFrameSink> frame_sink,
      GetCompositorFrameCallback get_compositor_frame_callback,
      OnFirstFrameRequestedCallback on_first_frame_requested_callback);

  FrameSinkHolder(const FrameSinkHolder&) = delete;
  FrameSinkHolder& operator=(const FrameSinkHolder&) = delete;

  ~FrameSinkHolder() override;

  // Delete `frame_sink_holder` after having reclaimed all exported resources.
  // Returns true if the holder will be deleted immediately.
  // TODO(reveman): Find a better way to handle deletion of in-flight resources.
  // https://crbug.com/765763
  static bool DeleteWhenLastResourceHasBeenReclaimed(
      std::unique_ptr<FrameSinkHolder> frame_sink_holder,
      aura::Window* host_window);

  void set_presentation_callback(PresentationCallback callback) {
    presentation_callback_ = std::move(callback);
  }

  base::WeakPtr<FrameSinkHolder> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // When auto-update mode is on, we keep on submitting frames asynchronously to
  // display compositor without a request to submit a frame via
  // `SubmitCompositorFrame()`.
  void SetAutoUpdateMode(bool mode);

  UiResourceManager& resource_manager() { return resources_manager_; }

  // Submits a single compositor frame to display compositor. Auto-submit
  // mode must be off to use this method. If synchronous_draw is true, we try to
  // submit frame to display compositor right away. Otherwise we will submit the
  // frame next time display compositor requests a new frame.
  // Note: In certain cases when we cannot submit frames right away, synchronous
  // requests will be changed to asynchronous requests.
  void SubmitCompositorFrame(bool synchronous_draw);

  // Overridden from cc::LayerTreeFrameSinkClient:
  void SetBeginFrameSource(viz::BeginFrameSource* source) override;
  std::optional<viz::HitTestRegionList> BuildHitTestData() override;
  void ReclaimResources(std::vector<viz::ReturnedResource> resources) override;
  void SetTreeActivationCallback(base::RepeatingClosure callback) override;
  void DidReceiveCompositorFrameAck() override;
  void DidPresentCompositorFrame(
      uint32_t frame_token,
      const viz::FrameTimingDetails& details) override;
  void DidLoseLayerTreeFrameSink() override;
  void OnDraw(const gfx::Transform& transform,
              const gfx::Rect& viewport,
              bool resourceless_software_draw,
              bool skip_draw) override;
  void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override;
  void SetExternalTilePriorityConstraints(
      const gfx::Rect& viewport_rect,
      const gfx::Transform& transform) override;

  // Overridden from viz::BeginFrameObserverBase:
  void OnBeginFrameSourcePausedChanged(bool paused) override;
  bool OnBeginFrameDerivedImpl(const viz::BeginFrameArgs& args) override;

  // Overridden from aura::WindowObserver
  void OnWindowDestroying(aura::Window* window) override;

 private:
  friend class FrameSinkHolderTestApi;

  void ObserveBeginFrameSource(bool start);

  // If we have not consecutively produced a frame in response to OnBeginFrame
  // events from the compositor, we can stop observing the
  // `begin_frame_source_`. This is because continuous polling from the
  // compositor and receiving DidNotProduceFrame responses from the client is
  // unnecessary work and can cause power regression.
  void MaybeStopObservingBeingFrameSource();

  void DidNotProduceFrame(viz::BeginFrameAck&& begin_frame_ack,
                          cc::FrameSkippedReason reason);

  // Create an empty frame that has dsf and size of the last submitted frame.
  viz::CompositorFrame CreateEmptyFrame();

  void SubmitCompositorFrameInternal(
      std::unique_ptr<viz::CompositorFrame> frame);

  void ScheduleDelete();

  // Returns true if we are waiting to reclaim all the exported resources after
  // which we schedule a delete task for the holder.
  bool WaitingToScheduleDelete() const;

  // Extend the lifetime of `this` by adding it as a observer to `root_window`.
  void SetRootWindowForDeletion(aura::Window* root_window);

  // True when the display compositor has already asked for a compositor
  // frame. This signifies that the gpu process has been fully initialized.
  bool first_frame_requested_ = false;

  // The layer tree frame sink created from `host_window_.
  std::unique_ptr<cc::LayerTreeFrameSink> frame_sink_;

  // The currently observed `BeginFrameSource` which will notify us with
  // `OnBeginFrameDerivedImpl()`.
  raw_ptr<viz::BeginFrameSource> begin_frame_source_ = nullptr;

  // True if we submitted a compositor frame and are waiting for a call to
  // `DidReceiveCompositorFrameAck()`.
  bool pending_compositor_frame_ack_ = false;

  // True if we asynchronously need to submit a compositor frame i.e submit a
  // frame next time display compositor requests for a new frame via
  // `OnBeginFrameDerivedImpl`.
  bool pending_compositor_frame_ = false;

  // The pixel size and the DSF of the most recently submitted compositor frame.
  // If either changes, we'll need to allocate a new local surface ID.
  gfx::Size last_frame_size_in_pixels_;
  float last_frame_device_scale_factor_ = 1.0f;

  // Keeps track of resources that are currently available to be reused in a
  // compositor frame and the resources that are in-use by the display
  // compositor.
  UiResourceManager resources_manager_;

  // Generates a frame token for the next compositor frame we create.
  viz::FrameTokenGenerator compositor_frame_token_generator_;

  // True if `this` is scheduled to be deleted.
  bool delete_pending_ = false;

  // When true, continuously submit frames asynchronously in the background.
  bool auto_update_ = false;

  // The callback to notify the client when surface contents have been
  // presented.
  PresentationCallback presentation_callback_;

  // The callback to generate the next compositor frame.
  GetCompositorFrameCallback get_compositor_frame_callback_;

  // The callback invoked when the display compositor asks for a compositor
  // frame for the first time.
  OnFirstFrameRequestedCallback on_first_frame_requested_callback_;

  // Observation of the root window to which this holder becomes an observer to
  // extend its lifespan till all the in-flight resource to display compositor
  // are reclaimed.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      root_window_observation_{this};
  base::ScopedObservation<viz::BeginFrameSource, viz::BeginFrameObserver>
      begin_frame_observation_{this};

  // The number of DidNotProduceFrame responses since the last time when a frame
  // is submitted.
  int consecutive_begin_frames_produced_no_frame_count_ = 0;

  base::WeakPtrFactory<FrameSinkHolder> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_FRAME_SINK_FRAME_SINK_HOLDER_H_
