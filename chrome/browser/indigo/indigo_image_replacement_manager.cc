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
#include "chrome/common/extensions/api/indigo_private.h"
#include "components/page_content_annotations/core/tracked_element_feature.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_event_histogram_value.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/mojom/event_dispatcher.mojom.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace indigo {

namespace {
InvocationId::Generator g_invocation_id_generator;
}

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
    if (primary_receiver_id_.has_value()) {
      // Registering a new primary replacement (when one was previously
      // registered) triggers a reset of all existing replacements.
      // Note: We don't want to reset the content script here as we're reacting
      // to it registering a new primary replacement.
      Reset(ResetType::kResetReplacementsOnly);
    }
    active_invocation_id_ = g_invocation_id_generator.GenerateNextId();
  } else if (!primary_receiver_id_.has_value()) {
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

  auto receiver_id = receivers_.Add(
      this, std::move(host_receiver),
      IndigoImageReplacement(this, std::move(remote), is_primary));
  if (is_primary) {
    primary_receiver_id_ = receiver_id;
  }
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
  primary_receiver_id_ = std::nullopt;
  primary_original_image_webp_bytes_.clear();
  generated_image_url_ = GURL();
  active_invocation_id_ = std::nullopt;
  CancelActiveRequest();
}

bool IndigoImageReplacementManager::RegenerateImage() {
  if (!primary_receiver_id_.has_value() ||
      !receivers_.HasReceiver(*primary_receiver_id_)) {
    return false;
  }

  CHECK(!primary_original_image_webp_bytes_.empty());

  // Reset generated image URL so subsequent getReplacementImage() requests
  // wait.
  generated_image_url_ = GURL();

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&page().GetMainDocument());
  content::BrowserContext* browser_context = web_contents->GetBrowserContext();
  auto event = std::make_unique<extensions::Event>(
      extensions::events::INDIGO_PRIVATE_ON_REGENERATE_STARTED,
      extensions::api::indigo_private::OnRegenerateStarted::kEventName,
      extensions::api::indigo_private::OnRegenerateStarted::Create());

  // Enable browser-side filtering by populating EventFilteringInfo with
  // instance_id (set to invocation_id).
  auto filter_info = extensions::mojom::EventFilteringInfo::New();
  filter_info->instance_id = active_invocation_id_->GetUnsafeValue();
  event->filter_info = std::move(filter_info);

  extensions::EventRouter::Get(browser_context)
      ->DispatchEventToExtension(extension_misc::kIndigoExtensionId,
                                 std::move(event));

  GenerateReplacementImage();
  return true;
}

std::optional<base::Token>
IndigoImageReplacementManager::GetPrimaryTrackedElementId() const {
  if (!primary_receiver_id_) {
    return std::nullopt;
  }
  const IndigoImageReplacement* image_replacement =
      receivers_.GetContext(*primary_receiver_id_);
  if (!image_replacement) {
    return std::nullopt;
  }
  CHECK(image_replacement->is_primary());
  return image_replacement->tracked_element_id();
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

  // Cache a copy of the primary replacement's original image bytes to use for
  // regeneration.
  if (original_image) {
    primary_original_image_webp_bytes_.assign_range(original_image->webp_bytes);
  }

  GenerateReplacementImage();
}

IndigoPageActionController*
IndigoImageReplacementManager::GetIndigoPageActionController() {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&page().GetMainDocument());
  auto* tab = tabs::TabInterface::GetFromContents(web_contents);
  if (!tab) {
    return nullptr;
  }
  auto* controller = indigo::IndigoPageActionController::From(tab);
  CHECK(controller);
  return controller;
}

void IndigoImageReplacementManager::GenerateReplacementImage() {
  CHECK(primary_receiver_id_.has_value());
  CHECK(!primary_original_image_webp_bytes_.empty());

  CancelActiveRequest();

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&page().GetMainDocument());
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  IndigoService* service = IndigoServiceFactory::GetForProfile(profile);
  if (service) {
    // Generate a new image based on the primary replacement's original image
    // bytes.
    cancel_active_request_ = service->GetApiClient().Generate(
        primary_original_image_webp_bytes_,
        base::BindOnce(
            &IndigoImageReplacementManager::OnReplacementImageGenerated,
            generate_weak_ptr_factory_.GetWeakPtr()));
  }
}

void IndigoImageReplacementManager::OnReplacementImageGenerated(
    base::expected<GeneratedImage, GenerateImageError> result) {
  CHECK(primary_receiver_id_.has_value());
  CHECK(receivers_.HasReceiver(*primary_receiver_id_));

  cancel_active_request_.Reset();

  if (!result.has_value()) {
    DVLOG(1) << "Generate image failed: " << result.error().message;
    base::UmaHistogramEnumeration(
        "Indigo.Transformation.Result",
        IndigoTransformationResult::kGenerateImageError);
    base::RecordAction(
        base::UserMetricsAction("Indigo.Transformation.Failure"));
    Reset(ResetType::kResetReplacementsAndContentScript);
    ShowErrorToast();
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

  if (auto* controller = GetIndigoPageActionController()) {
    controller->ShowToolbar();
  }
}

void IndigoImageReplacementManager::CancelActiveRequest() {
  generate_weak_ptr_factory_.InvalidateWeakPtrs();
  if (cancel_active_request_) {
    std::move(cancel_active_request_).Run();
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
    ShowErrorToast();
  }
}

void IndigoImageReplacementManager::Reset(ResetType reset_type) {
  if (auto* controller = GetIndigoPageActionController()) {
    controller->Reset(reset_type);
  }
}

void IndigoImageReplacementManager::ShowErrorToast() {
  if (auto* controller = GetIndigoPageActionController()) {
    controller->ShowInvocationErrorToast();
  }
}

PAGE_USER_DATA_KEY_IMPL(IndigoImageReplacementManager);

}  // namespace indigo
