// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_IMAGE_REPLACEMENT_H_
#define CHROME_BROWSER_INDIGO_INDIGO_IMAGE_REPLACEMENT_H_

#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/image_replacement/image_replacement.mojom.h"
#include "url/gurl.h"

namespace indigo {

class IndigoImageReplacementManager;

// Stores data associated with a single image replacement managed by
// `IndigoImageReplacementManager`. An instance of this class can be retrieved
// using `IndigoImageReplacementManager::GetImageReplacementForFrame`.
class IndigoImageReplacement {
 public:
  IndigoImageReplacement(IndigoImageReplacementManager* manager,
                         mojo::Remote<blink::mojom::ImageReplacement> remote,
                         bool is_primary);
  IndigoImageReplacement(IndigoImageReplacement&&);
  ~IndigoImageReplacement();

  content::FrameTreeNodeId frame_tree_node_id() const {
    return frame_tree_node_id_;
  }
  void ReplacementFrameAttached(
      content::FrameTreeNodeId frame_tree_node_id,
      std::vector<uint8_t> original_image_webp_bytes,
      const std::optional<base::Token>& tracked_element_id);
  bool is_primary() const { return is_primary_; }
  void ReplacementImageURLReady();

  const std::optional<base::Token>& tracked_element_id() const {
    return tracked_element_id_;
  }

  // Methods called by indigoPrivate extension functions:
  int32_t OnReadyToRender();
  std::vector<uint8_t> TakeOriginalImageWebpBytes();
  const GURL& GetReplacementImageURL() const;
  bool SetPendingReplacementImageCallback(
      base::OnceCallback<void(const GURL&)> callback);

 private:
  // `manager_` uniquely owns `this` and will outlive it.
  raw_ptr<IndigoImageReplacementManager> manager_;
  mojo::Remote<blink::mojom::ImageReplacement> remote_;
  // Identifies the replacement frame. It is only set after
  // ReplacementFrameAttached is called, and stays constant after.
  content::FrameTreeNodeId frame_tree_node_id_;
  std::vector<uint8_t> original_image_webp_bytes_;
  base::OnceCallback<void(const GURL&)> pending_replacement_image_callback_;
  const bool is_primary_;
  std::optional<base::Token> tracked_element_id_;
};

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_IMAGE_REPLACEMENT_H_
