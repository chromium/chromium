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

namespace content {
class Page;
}  // namespace content

namespace indigo {

class IndigoImageReplacementManager
    : public content::PageUserData<IndigoImageReplacementManager>,
      public blink::mojom::ImageReplacementHost {
 public:
  ~IndigoImageReplacementManager() override;

  IndigoImageReplacementManager(const IndigoImageReplacementManager&) = delete;
  IndigoImageReplacementManager& operator=(
      const IndigoImageReplacementManager&) = delete;

  void RegisterImageReplacement(
      mojo::PendingRemote<blink::mojom::ImageReplacement> image_replacement);
  IndigoImageReplacement* GetImageReplacementForFrame(
      const content::RenderFrameHost& rfh);

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

  mojo::ReceiverSet<blink::mojom::ImageReplacementHost, IndigoImageReplacement>
      receivers_;
  base::WeakPtrFactory<IndigoImageReplacementManager> weak_ptr_factory_{this};
};

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_IMAGE_REPLACEMENT_MANAGER_H_
