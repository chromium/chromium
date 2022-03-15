// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_REQUEST_H_
#define CC_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_REQUEST_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "cc/cc_export.h"
#include "cc/document_transition/document_transition_shared_element_id.h"
#include "components/viz/common/quads/compositor_frame_transition_directive.h"
#include "components/viz/common/quads/compositor_render_pass.h"

namespace cc {

// This class represents a document transition request. It is constructed in
// Blink with an intent of translating this request into a viz directive for the
// transition to occur.
class CC_EXPORT DocumentTransitionRequest {
 public:
  // Creates a Type::kCapture type of request.
  static std::unique_ptr<DocumentTransitionRequest> CreateCapture(
      uint32_t document_tag,
      uint32_t shared_element_count,
      std::vector<viz::SharedElementResourceId> capture_ids,
      base::OnceClosure commit_callback);

  // Creates a Type::kAnimateRenderer type of request.
  static std::unique_ptr<DocumentTransitionRequest> CreateAnimateRenderer(
      uint32_t document_tag);

  // Creates a Type::kRelease type of request.
  static std::unique_ptr<DocumentTransitionRequest> CreateRelease(
      uint32_t document_tag);

  DocumentTransitionRequest(DocumentTransitionRequest&) = delete;
  ~DocumentTransitionRequest();

  DocumentTransitionRequest& operator=(DocumentTransitionRequest&) = delete;

  // The callback is run when the request is sufficiently processed for us to be
  // able to begin the next step in the animation. In other words, when this
  // callback is invoked it can resolve a script promise that is gating this
  // step.
  base::OnceClosure TakeFinishedCallback() {
    return std::move(commit_callback_);
  }

  struct CC_EXPORT SharedElementInfo {
    SharedElementInfo();
    ~SharedElementInfo();

    viz::CompositorRenderPassId render_pass_id;
    viz::SharedElementResourceId resource_id;
  };
  // This constructs a viz directive. Note that repeated calls to this function
  // would create a new sequence id for the directive, which means it would be
  // processed again by viz.
  viz::CompositorFrameTransitionDirective ConstructDirective(
      const std::map<DocumentTransitionSharedElementId, SharedElementInfo>&
          shared_element_render_pass_id_map) const;

  // Returns the sequence id for this request.
  uint32_t sequence_id() const { return sequence_id_; }

  // Testing / debugging functionality.
  std::string ToString() const;

 private:
  using Type = viz::CompositorFrameTransitionDirective::Type;

  DocumentTransitionRequest(
      Type type,
      uint32_t document_tag,
      uint32_t shared_element_count,
      std::vector<viz::SharedElementResourceId> capture_ids,
      base::OnceClosure commit_callback);

  const Type type_;
  const uint32_t document_tag_;
  const uint32_t shared_element_count_;
  base::OnceClosure commit_callback_;
  const uint32_t sequence_id_;
  const std::vector<viz::SharedElementResourceId> capture_resource_ids_;

  static uint32_t s_next_sequence_id_;
};

}  // namespace cc

#endif  // CC_DOCUMENT_TRANSITION_DOCUMENT_TRANSITION_REQUEST_H_
