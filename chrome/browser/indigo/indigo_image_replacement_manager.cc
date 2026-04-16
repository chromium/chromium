// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_image_replacement_manager.h"

#include "base/check.h"
#include "base/logging.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "third_party/blink/public/common/tokens/tokens.h"

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
    const blink::LocalFrameToken& replacement_frame_token) {
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

  content::NavigationController::LoadURLParams params{
      extensions::Extension::GetResourceURL(
          extensions::Extension::GetBaseURLFromExtensionId(
              extension_misc::kIndigoExtensionId),
          "index.html")};
  params.frame_tree_node_id = image_replacement_subframe->GetFrameTreeNodeId();
  params.should_replace_current_entry = true;
  content::WebContents::FromRenderFrameHost(&page().GetMainDocument())
      ->GetController()
      .LoadURLWithParams(std::move(params));

  // TODO(b/489468738): We should wait for the extension to finish loading
  // before calling RenderReplacement.
  receivers_.current_context()->RenderReplacement();
}

PAGE_USER_DATA_KEY_IMPL(IndigoImageReplacementManager);

}  // namespace indigo
