// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_image_replacement.h"

#include "chrome/browser/indigo/indigo_image_replacement_manager.h"

namespace indigo {

IndigoImageReplacement::IndigoImageReplacement(
    IndigoImageReplacementManager* manager,
    mojo::Remote<blink::mojom::ImageReplacement> remote,
    bool is_primary)
    : manager_(manager), remote_(std::move(remote)), is_primary_(is_primary) {}

IndigoImageReplacement::IndigoImageReplacement(IndigoImageReplacement&&) =
    default;

IndigoImageReplacement::~IndigoImageReplacement() {
  if (pending_replacement_image_callback_) {
    std::move(pending_replacement_image_callback_).Run(GURL());
  }
}

void IndigoImageReplacement::ReplacementImageURLReady() {
  if (pending_replacement_image_callback_) {
    std::move(pending_replacement_image_callback_)
        .Run(manager_->generated_image_url());
  }
}

void IndigoImageReplacement::ReplacementFrameAttached(
    content::FrameTreeNodeId frame_tree_node_id,
    std::vector<uint8_t> original_image_webp_bytes) {
  CHECK(!frame_tree_node_id_);
  CHECK(original_image_webp_bytes_.empty());
  frame_tree_node_id_ = frame_tree_node_id;
  original_image_webp_bytes_ = std::move(original_image_webp_bytes);
}

void IndigoImageReplacement::OnReadyToRender() {
  remote_->RenderReplacement();
}

std::vector<uint8_t> IndigoImageReplacement::TakeOriginalImageWebpBytes() {
  return std::move(original_image_webp_bytes_);
}

const GURL& IndigoImageReplacement::GetReplacementImageURL() const {
  return manager_->generated_image_url();
}

bool IndigoImageReplacement::SetPendingReplacementImageCallback(
    base::OnceCallback<void(const GURL&)> callback) {
  if (!pending_replacement_image_callback_.is_null()) {
    return false;
  }
  pending_replacement_image_callback_ = std::move(callback);
  return true;
}

}  // namespace indigo
