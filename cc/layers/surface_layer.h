// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_LAYERS_SURFACE_LAYER_H_
#define CC_LAYERS_SURFACE_LAYER_H_

#include <memory>

#include "cc/cc_export.h"
#include "cc/layers/deadline_policy.h"
#include "cc/layers/layer.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/common/surfaces/surface_range.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class WaitableEvent;
}

namespace cc {

// If given true, we should submit frames, as we are unoccluded on screen.
// If given false, we should not submit compositor frames.
// The second parameter is only used in tests to ensure that the
// UpdateSubmissionStateCB is called synchronously relative to the calling
// thread. That is, the calling thread will block on the given waitable event
// when calling the callback. It is the responsibility of the callback to signal
// the event once the state has been updated. If blocking is not required, then
// the second parameter will be nullptr.
using UpdateSubmissionStateCB =
    base::RepeatingCallback<void(bool is_visible, base::WaitableEvent*)>;

// A layer that renders a surface referencing the output of another compositor
// instance or client.
class CC_EXPORT SurfaceLayer : public Layer {
 public:
  static scoped_refptr<SurfaceLayer> Create();
  static scoped_refptr<SurfaceLayer> Create(UpdateSubmissionStateCB);

  SurfaceLayer(const SurfaceLayer&) = delete;
  SurfaceLayer& operator=(const SurfaceLayer&) = delete;

  void SetSurfaceId(const viz::SurfaceId& surface_id,
                    const DeadlinePolicy& deadline_policy);
  void SetOldestAcceptableFallback(const viz::SurfaceId& surface_id);

  // When stretch_content_to_fill_bounds is true, the scale of the embedded
  // surface is ignored and the content will be stretched to fill the bounds.
  void SetStretchContentToFillBounds(bool stretch_content_to_fill_bounds);
  bool stretch_content_to_fill_bounds() const {
    return stretch_content_to_fill_bounds_.Read(*this);
  }

  void SetSurfaceHitTestable(bool surface_hit_testable);

  void SetHasPointerEventsNone(bool has_pointer_events_none);

  void SetIsReflection(bool is_reflection);

  void SetMayContainVideo(bool may_contain_video);

  // Layer overrides.
  std::unique_ptr<LayerImpl> CreateLayerImpl(
      LayerTreeImpl* tree_impl) const override;
  void SetLayerTreeHost(LayerTreeHost* host) override;
  void PushPropertiesTo(LayerImpl* layer,
                        const CommitState& commit_state,
                        const ThreadUnsafeCommitState& unsafe_state) override;

  const viz::SurfaceId& surface_id() const {
    return surface_range_.Read(*this).end();
  }

  const std::optional<viz::SurfaceId>& oldest_acceptable_fallback() const {
    return surface_range_.Read(*this).start();
  }

  std::optional<uint32_t> deadline_in_frames() const {
    return deadline_in_frames_.Read(*this);
  }

 protected:
  SurfaceLayer();
  explicit SurfaceLayer(UpdateSubmissionStateCB);
  bool HasDrawableContent() const override;

 private:
  ~SurfaceLayer() override;

  ProtectedSequenceWritable<UpdateSubmissionStateCB>
      update_submission_state_callback_;

  ProtectedSequenceReadable<bool> may_contain_video_;
  ProtectedSequenceReadable<viz::SurfaceRange> surface_range_;
  ProtectedSequenceWritable<std::optional<uint32_t>> deadline_in_frames_;

  ProtectedSequenceReadable<bool> stretch_content_to_fill_bounds_;

  // Whether or not the surface should submit hit test data when submitting
  // compositor frame. The bit represents that the surface layer may be
  // associated with an out-of-process iframe and viz hit testing needs to know
  // the hit test information of that iframe. This bit is different from a layer
  // being hit testable in the renderer, a hit testable surface layer may not
  // be surface hit testable (e.g., a surface layer created by video).
  ProtectedSequenceReadable<bool> surface_hit_testable_;

  // Whether or not the surface can accept pointer events. It is set to true if
  // the frame owner has pointer-events: none property.
  // TODO(sunxd): consider renaming it to oopif_has_pointer_events_none_ for
  // disambiguation.
  ProtectedSequenceWritable<bool> has_pointer_events_none_;

  // This surface layer is reflecting the root surface of another display.
  ProtectedSequenceReadable<bool> is_reflection_;

  // Keep track when we change LayerTreeHosts as SurfaceLayerImpl needs to know
  // in order to keep the visibility callback state consistent.
  ProtectedSequenceWritable<bool> callback_layer_tree_host_changed_;
};

}  // namespace cc

#endif  // CC_LAYERS_SURFACE_LAYER_H_
