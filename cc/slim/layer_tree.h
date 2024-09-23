// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SLIM_LAYER_TREE_H_
#define CC_SLIM_LAYER_TREE_H_

#include <cstdint>
#include <memory>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/presentation_feedback.h"

namespace cc {
class UIResourceManager;
}  // namespace cc

namespace viz {
class CopyOutputRequest;
class SurfaceId;
class SurfaceRange;
}

namespace cc::slim {

class FrameSink;
class Layer;
class LayerTreeClient;

// Hosts a tree of `slim::Layer`s. Responsible for generating and
// submitting frames and managing the lifetime of `FrameSink`s.
class COMPONENT_EXPORT(CC_SLIM) LayerTree {
 public:
  using SurfaceRangesAndCounts = base::flat_map<viz::SurfaceRange, int>;

  // A scoped object to keep a `viz::Surface` referenced, such that a
  // `CopyOutputRequest` can be made against it, even after the original
  // `SurfaceLayer` is destroyed.
  class ScopedKeepSurfaceAlive {
   public:
    ScopedKeepSurfaceAlive() = default;
    ScopedKeepSurfaceAlive(const ScopedKeepSurfaceAlive&) = delete;
    ScopedKeepSurfaceAlive& operator=(const ScopedKeepSurfaceAlive&) = delete;
    virtual ~ScopedKeepSurfaceAlive() = default;
  };

  static std::unique_ptr<LayerTree> Create(LayerTreeClient* client);

  virtual ~LayerTree() = default;

  // Sets root layer. Previous root layer is released.
  virtual void SetRoot(scoped_refptr<Layer> root) = 0;
  virtual const scoped_refptr<Layer>& root() const = 0;

  // Called in response to a `LayerTreeClient::RequestNewFrameSink` request
  // made to the client. The client will be informed of the LayerTreeFrameSink
  // initialization status using DidInitializeLayerTreeFrameSink or
  // DidFailToInitializeLayerTreeFrameSink.
  virtual void SetFrameSink(std::unique_ptr<FrameSink> sink) = 0;

  // Forces the host to immediately release all references to the
  // LayerTreeFrameSink, if any. Can be safely called any time, but the
  // compositor should not be visible.
  virtual void ReleaseLayerTreeFrameSink() = 0;

  // Returns the UIResourceManager used to create UIResources for
  // UIResourceLayers pushed to the LayerTree.
  virtual cc::UIResourceManager* GetUIResourceManager() = 0;

  // Sets viewport, scale, and local surface id where frames should be
  // submitted.
  virtual void SetViewportRectAndScale(
      const gfx::Rect& device_viewport_rect,
      float device_scale_factor,
      const viz::LocalSurfaceId& local_surface_id) = 0;

  // Set the background color used to fill in any areas in the viewport that is
  // not covered by the layer tree.
  virtual void set_background_color(SkColor4f color) = 0;

  // Sets or gets if the LayerTree is visible. When not visible it will:
  // - Not request a new LayerTreeFrameSink from the client.
  // - Stop `LayerTreeClient::BeginFrame` and submitting frames to the display
  //   compositor.
  // The LayerTree is not visible when first created, so this must be called
  // to make it visible before it will attempt to start producing output.
  virtual void SetVisible(bool visible) = 0;
  virtual bool IsVisible() const = 0;

  // Registers a callback that is run when the presentation feedback for the
  // next submitted frame is received (it's entirely possible some frames may be
  // dropped between the time this is called and the callback is run).
  // Note that since this might be called on failed presentations, it is
  // deprecated in favor of `RequestSuccessfulPresentationTimeForNextFrame()`
  // which will be called only after a successful presentation.
  using PresentationCallback =
      base::OnceCallback<void(const gfx::PresentationFeedback&)>;
  virtual void RequestPresentationTimeForNextFrame(
      PresentationCallback callback) = 0;

  // Registers a callback that is run when the next frame successfully makes it
  // to the screen (it's entirely possible some frames may be dropped between
  // the time this is called and the callback is run).
  using SuccessfulCallback =
      base::OnceCallback<void(const viz::FrameTimingDetails&)>;
  virtual void RequestSuccessfulPresentationTimeForNextFrame(
      SuccessfulCallback callback) = 0;

  // This the display transform hint. This generally does not affect visual
  // output but can affect how the display server can correctly use hardware
  // acceleration.
  virtual void set_display_transform_hint(gfx::OverlayTransform hint) = 0;

  // Request output of the layer tree. If the copy is unable to be produced
  // then the callback is called with a nullptr/empty result. If the
  // request's source property is set, any prior uncommitted requests having the
  // same source will be aborted.
  virtual void RequestCopyOfOutput(
      std::unique_ptr<viz::CopyOutputRequest> request) = 0;

  // Prevents the compositor from calling `LayerTreeClient::BeginFrame`
  // until the returned callback is called.
  virtual base::OnceClosure DeferBeginFrame() = 0;

  // Requests to produce a new frame even if no content has changed.
  // See `cc::LayerTreeHost` for distinction between this and
  // `SetNeedsRedrawForTesting()`.
  virtual void SetNeedsAnimate() = 0;

  // Works in combination with DelayedScheduler to indicate all the updates in
  // for a frame has arrived and a frame should be produced now. If compositor
  // is ready, it will immediately call `LayerTreeClient::BeginFrame`.
  // It is never a requirement to call MaybeCompositeNow every frame, and
  // calling it never guarantees a frame is produced immediately.
  // TODO(boliu): Move this method to DelayedScheduler once DelayedScheduler
  // moved out of slim.
  virtual void MaybeCompositeNow() = 0;

  virtual std::unique_ptr<ScopedKeepSurfaceAlive> CreateScopedKeepSurfaceAlive(
      const viz::SurfaceId& surface_id) = 0;

  // Exposes the ranges of referenced surfaces for testing.
  virtual const SurfaceRangesAndCounts& GetSurfaceRangesForTesting() const = 0;

  // Force to produce a compositor frame even if nothing has changed.
  virtual void SetNeedsRedrawForTesting() = 0;
};

}  // namespace cc::slim

#endif  // CC_SLIM_LAYER_TREE_H_
