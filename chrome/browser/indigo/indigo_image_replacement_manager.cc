// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_image_replacement_manager.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "chrome/browser/indigo/api_client.h"
#include "chrome/browser/indigo/indigo_image_replacement.h"
#include "chrome/browser/indigo/indigo_page_action_controller.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/indigo/indigo_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace indigo {

IndigoImageReplacementManager::IndigoImageReplacementManager(
    content::Page& page)
    : content::PageUserData<IndigoImageReplacementManager>(page) {
  CHECK(page.IsPrimary());
  receivers_.set_disconnect_handler(base::BindRepeating(
      &IndigoImageReplacementManager::OnReceiverDisconnected,
      base::Unretained(this)));
}

IndigoImageReplacementManager::~IndigoImageReplacementManager() = default;

void IndigoImageReplacementManager::RegisterImageReplacement(
    mojo::PendingRemote<blink::mojom::ImageReplacement> image_replacement,
    bool is_primary) {
  if (is_primary) {
    if (primary_registered_) {
      // Registering a new primary replacement (when one was previously
      // registered) triggers a reset of all existing replacements.
      ResetAllReplacements();
    }
    primary_registered_ = true;
  } else if (!primary_registered_) {
    // We ignore all non primary replacements until a primary replacement is
    // registered.
    return;
  }

  mojo::Remote<blink::mojom::ImageReplacement> remote(
      std::move(image_replacement));
  mojo::PendingRemote<blink::mojom::ImageReplacementHost> host_remote;
  auto host_receiver = host_remote.InitWithNewPipeAndPassReceiver();
  remote->StartReplacement(std::move(host_remote));
  receivers_.Add(this, std::move(host_receiver),
                 IndigoImageReplacement(this, std::move(remote), is_primary));
}

IndigoImageReplacement*
IndigoImageReplacementManager::GetImageReplacementForFrame(
    const content::RenderFrameHost& rfh) {
  content::FrameTreeNodeId frame_tree_node_id = rfh.GetFrameTreeNodeId();
  for (const auto& [receiver_id, context] : receivers_.GetAllContexts()) {
    if (context->frame_tree_node_id() == frame_tree_node_id) {
      return context;
    }
  }
  return nullptr;
}

void IndigoImageReplacementManager::ResetAllReplacements() {
  receivers_.Clear();
  primary_registered_ = false;
  generated_image_url_ = GURL();
}

void IndigoImageReplacementManager::ReplacementFrameAttached(
    const blink::LocalFrameToken& replacement_frame_token,
    const gfx::QuadF& quad,
    blink::mojom::ImageDataPtr original_image) {
  content::RenderFrameHost* image_replacement_subframe =
      content::RenderFrameHost::FromFrameToken(
          content::GlobalRenderFrameHostToken(
              page().GetMainDocument().GetProcess()->GetID().GetUnsafeValue(),
              replacement_frame_token));
  if (!image_replacement_subframe) {
    // TODO(b/489445294): We should wait for this subframe to be attached,
    // rather than returning early.
    LOG(ERROR) << "Subframe not found! " << replacement_frame_token.ToString();
    return;
  }

  if (image_replacement_subframe->GetParent() != &page().GetMainDocument()) {
    receivers_.ReportBadMessage(
        "Frame is not a child of the current document!");
    return;
  }

  auto& image_replacement = receivers_.current_context();
  if (image_replacement.frame_tree_node_id()) {
    receivers_.ReportBadMessage("Replacement frame already attached!");
    return;
  }

  content::FrameTreeNodeId frame_tree_node_id =
      image_replacement_subframe->GetFrameTreeNodeId();
  std::vector<uint8_t> image_bytes_copy;
  if (original_image) {
    image_bytes_copy.assign_range(original_image->webp_bytes);
  }
  image_replacement.ReplacementFrameAttached(frame_tree_node_id,
                                             std::move(image_bytes_copy));

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&page().GetMainDocument());

  content::NavigationController::LoadURLParams params{
      extensions::Extension::GetResourceURL(
          extensions::Extension::GetBaseURLFromExtensionId(
              extension_misc::kIndigoExtensionId),
          "index.html")};
  params.frame_tree_node_id = frame_tree_node_id;
  params.should_replace_current_entry = true;
  web_contents->GetController().LoadURLWithParams(std::move(params));

  // We only use the primary image for positioning the toolbar and generating
  // the replacement image. The generated image is then shared with the other
  // replacements.
  if (!image_replacement.is_primary()) {
    return;
  }

  gfx::QuadF scaled_quad = quad;
  if (content::RenderWidgetHostView* view =
          page().GetMainDocument().GetView()) {
    scaled_quad.Scale(1.0f / view->GetDeviceScaleFactor());
  }

  gfx::Rect bounds_rect = gfx::ToEnclosingRect(scaled_quad.BoundingBox());
  if (bounds_rect.IsEmpty()) {
    return;
  }

  if (auto* tab = tabs::TabInterface::GetFromContents(web_contents)) {
    // TODO(b/493707092): The controls should show up when the transformation
    // completes, rather than when it starts.
    if (auto* controller = indigo::IndigoPageActionController::From(tab)) {
      controller->ShowToolbarInside(bounds_rect);
    }
  }

  // Generate a new image based on the original image bytes.
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  IndigoService* service = IndigoServiceFactory::GetForProfile(profile);
  if (service) {
    service->GetApiClient().Generate(
        original_image->webp_bytes,
        base::BindOnce(
            &IndigoImageReplacementManager::OnReplacementImageGenerated,
            weak_ptr_factory_.GetWeakPtr(), receivers_.current_receiver()));
  }
}

void IndigoImageReplacementManager::OnReplacementImageGenerated(
    mojo::ReceiverId receiver_id,
    base::expected<GeneratedImage, GenerateImageError> result) {
  if (!receivers_.HasReceiver(receiver_id)) {
    return;
  }
  CHECK(receivers_.GetContext(receiver_id)->is_primary());

  if (!result.has_value()) {
    LOG(ERROR) << "Generate image failed: " << result.error().message;
    ResetAllReplacements();
    return;
  }

  CHECK(result->image_url.is_valid());
  generated_image_url_ = result->image_url;

  for (auto& [_, image_replacement] : receivers_.GetAllContexts()) {
    image_replacement->ReplacementImageURLReady();
  }
}

void IndigoImageReplacementManager::OnReceiverDisconnected() {
  IndigoImageReplacement& replacement = receivers_.current_context();
  // If the primary replacement is disconnected prior to receiving the generated
  // image, we reset all replacements.
  if (replacement.is_primary() && generated_image_url_.is_empty()) {
    LOG(ERROR) << "Primary image replacement disconnected before receiving "
                  "generated image";
    ResetAllReplacements();
  }
}

PAGE_USER_DATA_KEY_IMPL(IndigoImageReplacementManager);

}  // namespace indigo
