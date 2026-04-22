// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_image_replacement_manager.h"

#include "base/check.h"
#include "base/logging.h"
#include "chrome/browser/indigo/api_client.h"
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
}

IndigoImageReplacementManager::~IndigoImageReplacementManager() = default;

void IndigoImageReplacementManager::RegisterImageReplacement(
    mojo::PendingRemote<blink::mojom::ImageReplacement> image_replacement) {
  mojo::Remote<blink::mojom::ImageReplacement> remote(
      std::move(image_replacement));
  mojo::PendingRemote<blink::mojom::ImageReplacementHost> host_remote;
  auto host_receiver = host_remote.InitWithNewPipeAndPassReceiver();
  remote->StartReplacement(std::move(host_remote));
  receivers_.Add(this, std::move(host_receiver), std::move(remote));
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

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&page().GetMainDocument());
  content::NavigationController::LoadURLParams params{
      extensions::Extension::GetResourceURL(
          extensions::Extension::GetBaseURLFromExtensionId(
              extension_misc::kIndigoExtensionId),
          "index.html")};
  params.frame_tree_node_id = image_replacement_subframe->GetFrameTreeNodeId();
  params.should_replace_current_entry = true;
  web_contents->GetController().LoadURLWithParams(std::move(params));

  // TODO(b/489468738): We should wait for the extension to finish loading
  // before calling RenderReplacement.
  receivers_.current_context()->RenderReplacement();

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
            [](base::WeakPtr<content::WebContents> web_contents,
               base::expected<GeneratedImage, GenerateImageError> result) {
              if (!web_contents) {
                return;
              }
              if (!result.has_value()) {
                LOG(ERROR) << "Generate image failed: "
                           << result.error().message;
                return;
              }

              // Open the resulting data URL in a new tab.
              // This is temporary until we can load it into the replacement
              // frame.
              content::OpenURLParams open_params(
                  result->image_url, content::Referrer(),
                  WindowOpenDisposition::NEW_FOREGROUND_TAB,
                  ui::PAGE_TRANSITION_LINK, /*is_renderer_initiated=*/false);
              web_contents->OpenURL(open_params, base::NullCallback());
            },
            web_contents->GetWeakPtr()));
  }
}

PAGE_USER_DATA_KEY_IMPL(IndigoImageReplacementManager);

}  // namespace indigo
