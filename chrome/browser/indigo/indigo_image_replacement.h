// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_IMAGE_REPLACEMENT_H_
#define CHROME_BROWSER_INDIGO_INDIGO_IMAGE_REPLACEMENT_H_

#include <vector>

#include "base/functional/callback.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/image_replacement/image_replacement.mojom.h"
#include "url/gurl.h"

namespace indigo {

// Stores data associated with a single image replacement managed by
// `IndigoImageReplacementManager`. An instance of this class can be retrieved
// using `IndigoImageReplacementManager::GetImageReplacementForFrame`.
class IndigoImageReplacement {
 public:
  explicit IndigoImageReplacement(
      mojo::Remote<blink::mojom::ImageReplacement> remote);
  IndigoImageReplacement(IndigoImageReplacement&&);
  ~IndigoImageReplacement();

  content::FrameTreeNodeId frame_tree_node_id() const {
    return frame_tree_node_id_;
  }
  void ReplacementFrameAttached(content::FrameTreeNodeId frame_tree_node_id,
                                std::vector<uint8_t> original_image_webp_bytes);
  void SetReplacementImageUrl(GURL replacement_image_url);

  // Methods called by indigoPrivate extension functions:
  void OnReadyToRender();
  std::vector<uint8_t> TakeOriginalImageWebpBytes();
  GURL TakeReplacementImageURL();
  bool SetPendingReplacementImageCallback(
      base::OnceCallback<void(GURL)> callback);

 private:
  mojo::Remote<blink::mojom::ImageReplacement> remote_;
  // Identifies the replacement frame. It is only set after
  // ReplacementFrameAttached is called, and stays constant after.
  content::FrameTreeNodeId frame_tree_node_id_;
  std::vector<uint8_t> original_image_webp_bytes_;
  GURL replacement_image_url_;
  base::OnceCallback<void(GURL)> pending_replacement_image_callback_;
};

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_IMAGE_REPLACEMENT_H_
