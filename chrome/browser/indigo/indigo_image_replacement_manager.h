// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INDIGO_INDIGO_IMAGE_REPLACEMENT_MANAGER_H_
#define CHROME_BROWSER_INDIGO_INDIGO_IMAGE_REPLACEMENT_MANAGER_H_

#include <optional>
#include <vector>

#include "base/types/expected.h"
#include "base/types/id_type.h"
#include "base/types/pass_key.h"
#include "chrome/browser/indigo/api_client.h"
#include "chrome/browser/indigo/indigo_image_replacement.h"
#include "content/public/browser/page_user_data.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/image_replacement/image_replacement.mojom.h"
#include "ui/gfx/geometry/quad_f.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace content {
class Page;
}  // namespace content

namespace indigo {

class IndigoPageActionController;
enum class ResetType;

// An identifier used to group a set of related (primary and non-primary)
// image replacements. This ID is currently shared with the Indigo component
// extension and is used to filter events.
using InvocationId = base::IdType32<class InvocationIdTag>;

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
  // Resets all image replacements owned and managed by this class.
  void ResetAllReplacements(base::PassKey<IndigoPageActionController>);
  // Returns true if a new generate request is sent, false otherwise.
  // Note: This does not wait for the request to complete.
  bool RegenerateImage();
  const GURL& generated_image_url() const { return generated_image_url_; }
  std::optional<base::Token> GetPrimaryTrackedElementId() const;
  std::optional<InvocationId> active_invocation_id() const {
    return active_invocation_id_;
  }

  // blink::mojom::ImageReplacementHost implementation:
  void ReplacementFrameAttached(
      const blink::LocalFrameToken& replacement_frame_token,
      blink::mojom::ImageDataPtr original_image,
      const std::optional<base::Token>& tracked_element_id) override;

 private:
  friend class content::PageUserData<IndigoImageReplacementManager>;
  PAGE_USER_DATA_KEY_DECL();

  explicit IndigoImageReplacementManager(content::Page& page);

  void GenerateReplacementImage();
  void OnReplacementImageGenerated(
      base::expected<GeneratedImage, GenerateImageError> result);
  void CancelActiveRequest();
  void OnReceiverDisconnected();
  void Reset(ResetType reset_type);

  mojo::ReceiverSet<blink::mojom::ImageReplacementHost, IndigoImageReplacement>
      receivers_;
  std::optional<mojo::ReceiverId> primary_receiver_id_;
  GURL generated_image_url_;
  std::vector<uint8_t> primary_original_image_webp_bytes_;
  std::optional<InvocationId> active_invocation_id_;
  base::OnceClosure cancel_active_request_;
  // This WeakPtr factory is specifically used when creating a callback when
  // starting a generate request. Prefer using `weak_ptr_factory_` for other
  // purposes.
  base::WeakPtrFactory<IndigoImageReplacementManager>
      generate_weak_ptr_factory_{this};
  base::WeakPtrFactory<IndigoImageReplacementManager> weak_ptr_factory_{this};
};

}  // namespace indigo

#endif  // CHROME_BROWSER_INDIGO_INDIGO_IMAGE_REPLACEMENT_MANAGER_H_
