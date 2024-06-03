// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_VIEW_TRANSITION_VIEW_TRANSITION_REQUEST_H_
#define CC_VIEW_TRANSITION_VIEW_TRANSITION_REQUEST_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "cc/cc_export.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/quads/compositor_render_pass.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "ui/gfx/display_color_spaces.h"

namespace cc {

// This class represents a view transition request. It is constructed in
// Blink with an intent of translating this request into a viz directive for the
// transition to occur.
class CC_EXPORT ViewTransitionRequest {
 public:
  using ViewTransitionElementMap =
      std::map<viz::ViewTransitionElementResourceId,
               viz::CompositorRenderPassId>;
  using Type = viz::CompositorFrameTransitionDirective::Type;

  // Creates a Type::kCapture type of request.
  // `transition_token` is an identifier that uniquely identifies each
  // transition.
  // `maybe_cross_frame_sink` is set if this transition can start and animate on
  // different CompositorFrameSink instances (i.e. cross-document navigations).
  static std::unique_ptr<ViewTransitionRequest> CreateCapture(
      const blink::ViewTransitionToken& transition_token,
      bool maybe_cross_frame_sink,
      std::vector<viz::ViewTransitionElementResourceId> capture_ids,
      base::OnceClosure commit_callback);

  // Creates a Type::kAnimateRenderer type of request.
  static std::unique_ptr<ViewTransitionRequest> CreateAnimateRenderer(
      const blink::ViewTransitionToken& transition_token,
      bool maybe_cross_frame_sink);

  // Creates a Type::kRelease type of request.
  static std::unique_ptr<ViewTransitionRequest> CreateRelease(
      const blink::ViewTransitionToken& transition_token,
      bool maybe_cross_frame_sink);

  ViewTransitionRequest(ViewTransitionRequest&) = delete;
  ~ViewTransitionRequest();

  ViewTransitionRequest& operator=(ViewTransitionRequest&) = delete;

  // The callback is run when the request is sufficiently processed for us to be
  // able to begin the next step in the animation. In other words, when this
  // callback is invoked it can resolve a script promise that is gating this
  // step.
  base::OnceClosure TakeFinishedCallback() {
    return std::move(commit_callback_);
  }

  // This constructs a viz directive. Note that repeated calls to this function
  // would create a new sequence id for the directive, which means it would be
  // processed again by viz.
  viz::CompositorFrameTransitionDirective ConstructDirective(
      const ViewTransitionElementMap& shared_element_render_pass_id_map,
      const gfx::DisplayColorSpaces& display_color_spaces) const;

  // Returns the sequence id for this request.
  uint32_t sequence_id() const { return sequence_id_; }

  Type type() const { return type_; }

  // Testing / debugging functionality.
  std::string ToString() const;

 private:
  ViewTransitionRequest(
      Type type,
      const blink::ViewTransitionToken& transition_token,
      bool maybe_cross_frame_sink,
      std::vector<viz::ViewTransitionElementResourceId> capture_ids,
      base::OnceClosure commit_callback);

  const Type type_;
  const blink::ViewTransitionToken transition_token_;
  const bool maybe_cross_frame_sink_;
  base::OnceClosure commit_callback_;
  const uint32_t sequence_id_;
  const std::vector<viz::ViewTransitionElementResourceId> capture_resource_ids_;

  static uint32_t s_next_sequence_id_;
};

}  // namespace cc

#endif  // CC_VIEW_TRANSITION_VIEW_TRANSITION_REQUEST_H_
