// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/indigo/indigo_image_replacement_manager.h"

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "chrome/browser/indigo/api_client.h"
#include "chrome/browser/indigo/indigo_image_replacement.h"
#include "chrome/browser/indigo/indigo_page_action_controller.h"
#include "chrome/browser/indigo/indigo_service.h"
#include "chrome/browser/indigo/indigo_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/page_content_annotations/core/tracked_element_feature.h"
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
  CHECK(base::FeatureList::IsEnabled(features::kIndigo));
  CHECK(tabs::TabInterface::GetFromContents(
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument())));
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
      // Note: We don't want to reset the content script here as we're reacting
      // to it registering a new primary replacement.
      Reset(ResetType::kResetReplacementsOnly);
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
  std::optional<int32_t> feature_id;
  if (is_primary) {
    feature_id =
        static_cast<int32_t>(page_content_annotations::TrackedElementFeature::
                                 kIndigoImageReplacement);
  }
  remote->StartReplacement(std::move(host_remote), feature_id);
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

void IndigoImageReplacementManager::ResetAllReplacements(
    base::PassKey<IndigoPageActionController>) {
  receivers_.Clear();
  primary_registered_ = false;
  generated_image_url_ = GURL();
}

std::optional<base::Token>
IndigoImageReplacementManager::GetPrimaryTrackedElementId() const {
  for (const auto& [receiver_id, context] : receivers_.GetAllContexts()) {
    if (context->is_primary()) {
      return context->tracked_element_id();
    }
  }
  return std::nullopt;
}

void IndigoImageReplacementManager::ReplacementFrameAttached(
    const blink::LocalFrameToken& replacement_frame_token,
    blink::mojom::ImageDataPtr original_image,
    const std::optional<base::Token>& tracked_element_id) {
  content::RenderFrameHost* image_replacement_subframe =
      content::RenderFrameHost::FromFrameToken(
          content::GlobalRenderFrameHostToken(
              page().GetMainDocument().GetProcess()->GetID().GetUnsafeValue(),
              replacement_frame_token));
  if (!image_replacement_subframe) {
    // TODO(b/489445294): We should wait for this subframe to be attached,
    // rather than returning early.
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
  image_replacement.ReplacementFrameAttached(
      frame_tree_node_id, std::move(image_bytes_copy), tracked_element_id);

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

  // We only use the primary image for generating the replacement image.
  // The generated image is then shared with the other replacements.
  if (!image_replacement.is_primary()) {
    return;
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

  IndigoPageActionController* controller = nullptr;
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&page().GetMainDocument());
  if (auto* tab = tabs::TabInterface::GetFromContents(web_contents)) {
    controller = indigo::IndigoPageActionController::From(tab);
    CHECK(controller);
  }

  if (!result.has_value()) {
    DVLOG(1) << "Generate image failed: " << result.error().message;
    base::UmaHistogramEnumeration(
        "Indigo.Transformation.Result",
        IndigoTransformationResult::kGenerateImageError);
    base::RecordAction(
        base::UserMetricsAction("Indigo.Transformation.Failure"));
    Reset(ResetType::kResetReplacementsAndContentScript);
    if (controller) {
      controller->ShowInvocationErrorToast();
    }
    return;
  }

  CHECK(result->image_url.is_valid());
  generated_image_url_ = result->image_url;

  base::UmaHistogramEnumeration("Indigo.Transformation.Result",
                                IndigoTransformationResult::kSuccess);
  base::RecordAction(base::UserMetricsAction("Indigo.Transformation.Success"));

  for (auto& [_, image_replacement] : receivers_.GetAllContexts()) {
    image_replacement->ReplacementImageURLReady();
  }

  if (controller) {
    controller->ShowToolbar();
  }
}

void IndigoImageReplacementManager::OnReceiverDisconnected() {
  IndigoImageReplacement& replacement = receivers_.current_context();
  // If the primary replacement is disconnected prior to receiving the generated
  // image, we reset all replacements.
  if (replacement.is_primary() && generated_image_url_.is_empty()) {
    DVLOG(1) << "Primary image replacement disconnected before receiving "
                "generated image";
    Reset(ResetType::kResetReplacementsAndContentScript);
  }
}

void IndigoImageReplacementManager::Reset(ResetType reset_type) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&page().GetMainDocument());
  if (auto* tab = tabs::TabInterface::GetFromContents(web_contents)) {
    auto* controller = indigo::IndigoPageActionController::From(tab);
    CHECK(controller);
    controller->Reset(reset_type);
  }
}

PAGE_USER_DATA_KEY_IMPL(IndigoImageReplacementManager);

}  // namespace indigo
