// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_IMAGE_REPLACEMENT_MANAGER_H_
#define CHROME_BROWSER_INDIGO_INDIGO_IMAGE_REPLACEMENT_MANAGER_H_

#include "base/types/expected.h"
#include "chrome/browser/indigo/api_client.h"
#include "chrome/browser/indigo/indigo_image_replacement.h"
#include "content/public/browser/page_user_data.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/image_replacement/image_replacement.mojom.h"
#include "ui/gfx/geometry/quad_f.h"
#include "url/gurl.h"

namespace content {
class Page;
}  // namespace content

namespace indigo {

// Manages a group of related ImageReplacements created by the content script
// run by IndigoAgent. The manager uses the "primary" ImageReplacement's
// original image's bytes to generate the replacement image. The replacement
// image is then shared with all active ImageReplacements. There can only be
// one "primary" ImageReplacement registered at any given time.
//
// All non-primary image replacements are ignored (and dropped) until the first
// primary image replacement is registered. If the primary image replacement is
// disconnected prior to the generated image being available, all replacements
// are reset and the manager goes back to waiting for the next primary image
// replacement. When a subsequent primary replacement is registered, the
// previous primary replacement and all non-primary replacements are
// disconnected before processing the new primary replacement.
class IndigoImageReplacementManager
    : public content::PageUserData<IndigoImageReplacementManager>,
      public blink::mojom::ImageReplacementHost {
 public:
  ~IndigoImageReplacementManager() override;

  IndigoImageReplacementManager(const IndigoImageReplacementManager&) = delete;
  IndigoImageReplacementManager& operator=(
      const IndigoImageReplacementManager&) = delete;

  void RegisterImageReplacement(
      mojo::PendingRemote<blink::mojom::ImageReplacement> image_replacement,
      bool is_primary);
  IndigoImageReplacement* GetImageReplacementForFrame(
      const content::RenderFrameHost& rfh);
  void ResetAllReplacements();
  const GURL& generated_image_url() const { return generated_image_url_; }

  // blink::mojom::ImageReplacementHost implementation:
  void ReplacementFrameAttached(
      const blink::LocalFrameToken& replacement_frame_token,
      const gfx::QuadF& quad,
      blink::mojom::ImageDataPtr original_image) override;

 private:
  friend class content::PageUserData<IndigoImageReplacementManager>;
  PAGE_USER_DATA_KEY_DECL();

  explicit IndigoImageReplacementManager(content::Page& page);

  void OnReplacementImageGenerated(
      mojo::ReceiverId receiver_id,
      base::expected<GeneratedImage, GenerateImageError> result);
  void OnReceiverDisconnected();

  mojo::ReceiverSet<blink::mojom::ImageReplacementHost, IndigoImageReplacement>
      receivers_;
  bool primary_registered_ = false;
  GURL generated_image_url_;
  base::WeakPtrFactory<IndigoImageReplacementManager> weak_ptr_factory_{this};
};

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_IMAGE_REPLACEMENT_MANAGER_H_
