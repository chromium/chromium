// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_image_replacement.h"

namespace indigo {

IndigoImageReplacement::IndigoImageReplacement(
    mojo::Remote<blink::mojom::ImageReplacement> remote)
    : remote_(std::move(remote)) {}

IndigoImageReplacement::IndigoImageReplacement(IndigoImageReplacement&&) =
    default;

IndigoImageReplacement::~IndigoImageReplacement() = default;

void IndigoImageReplacement::ReplacementFrameAttached(
    content::FrameTreeNodeId frame_tree_node_id) {
  CHECK(!frame_tree_node_id_);
  frame_tree_node_id_ = frame_tree_node_id;
}

void IndigoImageReplacement::OnReadyToRender() {
  remote_->RenderReplacement();
}

}  // namespace indigo
