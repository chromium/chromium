// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_share_image_handler.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_metadata.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#endif

namespace glic {

namespace {

constexpr int kShareThumbnailMinSize = 500 * 500;
constexpr int kShareThumbnailMaxWidth = 1000;
constexpr int kShareThumbnailMaxHeight = 1000;
constexpr int kShareThumbnailMaxSize =
    kShareThumbnailMaxWidth * kShareThumbnailMaxHeight * 4;

mojom::AdditionalContextPtr CreateAdditionalContext(
    const GURL& src_url,
    const GURL& frame_url,
    const url::Origin& frame_origin,
    base::span<const uint8_t> thumbnail_data,
    tabs::TabHandle handle,
    const std::string& mime_type) {
  // TODO(b:448726704): update to use an Image part.
  auto context = glic::mojom::AdditionalContext::New();
  std::vector<glic::mojom::AdditionalContextPartPtr> parts;
  auto context_data = mojom::ContextData::New();
  context->source = glic::mojom::AdditionalContextSource::kShareContextMenu;
  context_data->mime_type = mime_type;
  context_data->data = mojo_base::BigBuffer(thumbnail_data);
  parts.push_back(
      mojom::AdditionalContextPart::NewData(std::move(context_data)));
  context->name = src_url.spec();
  context->tab_id = handle.raw_value();
  context->origin = frame_origin;
  context->frameUrl = frame_url;
  context->parts = std::move(parts);
  return context;
}

}  // namespace

GlicShareImageHandler::GlicShareImageHandler(GlicKeyedService& service)
    : service_(service) {}

GlicShareImageHandler::~GlicShareImageHandler() = default;

void GlicShareImageHandler::ShareContextImage(
    tabs::TabInterface* tab,
    content::RenderFrameHost* render_frame_host,
    const GURL& src_url) {
  if (!tab) {
    service_->metrics()->OnShareImageComplete(ShareImageResult::kFailedNoTab);
    return;
  }

  if (!render_frame_host) {
    MaybeShowErrorToast(tab);
    service_->metrics()->OnShareImageComplete(ShareImageResult::kFailedNoFrame);
    return;
  }

  if (is_share_in_progress_) {
    // Cancel the previous attempt at sharing.
    ShareComplete(ShareImageResult::kFailedReplacedByNewShare);
  }

  Reset();
  is_share_in_progress_ = true;
  service_->metrics()->OnShareImageStarted();

  tab_handle_ = tab->GetHandle();
  src_url_ = src_url;
  frame_url_ = render_frame_host->GetLastCommittedURL();
  frame_origin_ = render_frame_host->GetLastCommittedOrigin();
  render_frame_host_id_ = render_frame_host->GetGlobalId();

  MaybeStartObservingNavigation(tab);

  if (!MaybeStartObservingEligibility(tab)) {
    ShareComplete(ShareImageResult::kFailedNoTabContext);
    return;
  }

  // Store the InterfacePtr into the callback so that it's kept alive until
  // there's either a connection error or a response.
  chrome_render_frame_remote_ = std::make_unique<
      mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>>();
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      chrome_render_frame_remote_.get());

  chrome_render_frame_remote_->get()->RequestImageForContextNode(
      kShareThumbnailMinSize,
      gfx::Size(kShareThumbnailMaxWidth, kShareThumbnailMaxHeight),
      // TODO(b:448715912): consider other formats.
      chrome::mojom::ImageFormat::PNG, chrome::mojom::kDefaultQuality,
      base::BindOnce(&GlicShareImageHandler::OnReceivedImage,
                     // Can use Unretained here, because we reset the remote
                     // in `Reset`.
                     base::Unretained(this)));
}

void GlicShareImageHandler::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  ShareComplete(ShareImageResult::kFailedSawNavigation);
}

void GlicShareImageHandler::OnWillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  ShareComplete(ShareImageResult::kFailedDiscardedContents);
}

void GlicShareImageHandler::OnWillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  ShareComplete(ShareImageResult::kFailedDetachedTab);
}

void GlicShareImageHandler::OnReceivedImage(
    const std::vector<uint8_t>& thumbnail_data,
    const gfx::Size& original_size,
    const gfx::Size& downscaled_size,
    const std::string& mime_type,
    std::vector<lens::mojom::LatencyLogPtr> log_data) {
  // Close the remote since we've received our thumbnail.
  chrome_render_frame_remote_.reset();

  // Also stop observing eligibility.
  eligibility_subscription_ = base::CallbackListSubscription();

  if (thumbnail_data.empty()) {
    ShareComplete(ShareImageResult::kFailedNoImage);
    return;
  }

  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab) {
    ShareComplete(ShareImageResult::kFailedNoTab);
    return;
  }

  auto* rfh = content::RenderFrameHost::FromID(render_frame_host_id_);
  if (!rfh) {
    ShareComplete(ShareImageResult::kFailedNoFrame);
    return;
  }

  auto additional_context =
      CreateAdditionalContext(src_url_, frame_url_, frame_origin_,
                              thumbnail_data, tab_handle_, mime_type);

  content::ClipboardEndpoint source(
      ui::DataTransferEndpoint(
          rfh->GetMainFrame()->GetLastCommittedURL(),
          {.off_the_record = rfh->GetBrowserContext()->IsOffTheRecord()}),
      base::BindRepeating(
          [](content::GlobalRenderFrameHostId rfh_id)
              -> content::BrowserContext* {
            auto* rfh = content::RenderFrameHost::FromID(rfh_id);
            return rfh ? rfh->GetBrowserContext() : nullptr;
          },
          rfh->GetGlobalId()),
      *rfh);

  ui::ClipboardMetadata metadata;
  metadata.format_type = ui::ClipboardFormatType::PngType();
  metadata.size = thumbnail_data.size();

  bool do_policy_checks =
      AreClipboardPolicyChecksRequired(thumbnail_data.size());
  PolicyCheck policy_check =
      do_policy_checks ? PolicyCheck::kClipboard : PolicyCheck::kNone;

  GlicInvokeOptions invoke_options(Target(*tab, NewConversation()),
                                   mojom::InvocationSource::kSharedImage);
  invoke_options.additional_context = AdditionalTabContext(
      std::move(additional_context), render_frame_host_id_, policy_check);
  invoke_options.fre_override = mojom::FreOverride::kTrustFirstClick;
  invoke_options.fre_completion_wait_mode = FreCompletionWaitMode::kNever;
  invoke_options.on_error = base::BindOnce(
      &GlicShareImageHandler::OnInvokeError, weak_ptr_factory_.GetWeakPtr());
  invoke_options.on_success = base::BindOnce(
      &GlicShareImageHandler::ShareComplete, weak_ptr_factory_.GetWeakPtr(),
      ShareImageResult::kSentImageToClient);
  service_->Invoke(std::move(invoke_options));
  StopObservingNavigation();
}

void GlicShareImageHandler::OnInvokeError(GlicInvokeError error) {
  switch (error) {
    case GlicInvokeError::kUnknown:
      ShareComplete(ShareImageResult::kFailedUnknown);
      break;
    case GlicInvokeError::kTimeout:
      ShareComplete(ShareImageResult::kFailedTimedOut);
      break;
    case GlicInvokeError::kInvalidConversationId:
      ShareComplete(ShareImageResult::kFailedInvalidConversationId);
      break;
    case GlicInvokeError::kInvalidTab:
      ShareComplete(ShareImageResult::kFailedNoTab);
      break;
    case GlicInvokeError::kTabClosed:
      ShareComplete(ShareImageResult::kFailedNoTab);
      break;
    case GlicInvokeError::kInstanceDestroyed:
      ShareComplete(ShareImageResult::kFailedLostInstance);
      break;
    case GlicInvokeError::kInvokeInProgress:
      ShareComplete(ShareImageResult::kFailedInvokeInProgress);
      break;
    case GlicInvokeError::kInvalidConfiguration:
      ShareComplete(ShareImageResult::kFailedInvalidConfiguration);
      break;
    case GlicInvokeError::kAdditionalContextSawNavigation:
      ShareComplete(ShareImageResult::kFailedSawNavigation);
      break;
    case GlicInvokeError::kAdditionalContextFailedCopyPolicy:
      ShareComplete(ShareImageResult::kFailedClipboardCopyPolicy);
      break;
    case GlicInvokeError::kAdditionalContextFailedPastePolicy:
      ShareComplete(ShareImageResult::kFailedClipboardPastePolicy);
      break;
    case GlicInvokeError::kAdditionalContextNoSourceFrame:
      ShareComplete(ShareImageResult::kFailedNoFrame);
      break;
    case GlicInvokeError::kAdditionalContextNoClientFrame:
      ShareComplete(ShareImageResult::kFailedNoClientFrame);
      break;
    case GlicInvokeError::kAdditionalContextNoClipboardMetadata:
      ShareComplete(ShareImageResult::kFailedNoClipboardMetadata);
      break;
    case GlicInvokeError::kInstanceNotFound:
      ShareComplete(ShareImageResult::kFailedLostInstance);
      break;
    default:
      ShareComplete(ShareImageResult::kFailedUnknown);
      break;
  }
}

void GlicShareImageHandler::ShareComplete(ShareImageResult result) {
  if (result != ShareImageResult::kSentImageToClient &&
      result != ShareImageResult::kFailedClipboardPastePolicy &&
      result != ShareImageResult::kFailedClipboardCopyPolicy) {
    // Policy checks already show UI when they fail and don't need a toast.
    MaybeShowErrorToast(tab_handle_.Get());
  }
  service_->metrics()->OnShareImageComplete(result);
  Reset();
}

void GlicShareImageHandler::MaybeShowErrorToast(tabs::TabInterface* tab) {
  if (!tab) {
    return;
  }
#if !BUILDFLAG(IS_ANDROID)  // TODO(b/478008740): Implement for android.
  if (BrowserWindowInterface* browser = tab->GetBrowserWindowInterface()) {
    if (auto* controller = browser->GetFeatures().toast_controller()) {
      controller->MaybeShowToast(ToastParams(ToastId::kGlicShareImageFailed));
    }
  }
#endif
}

void GlicShareImageHandler::StopObservingNavigation() {
  // Ensure we're not observing any WebContents.
  Observe(nullptr);

  // Ensure we don't subscribe to discards of this WebContents.
  will_discard_web_contents_subscription_ = base::CallbackListSubscription();

  // Ensure we don't subscribe to tab detachment.
  will_detach_subscription_ = base::CallbackListSubscription();
}

bool GlicShareImageHandler::AreClipboardPolicyChecksRequired(
    std::optional<size_t> size) {
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_frame_host_id_);
  if (!rfh) {
    return false;
  }

  content::ClipboardEndpoint source(
      ui::DataTransferEndpoint(
          rfh->GetMainFrame()->GetLastCommittedURL(),
          {.off_the_record = rfh->GetBrowserContext()->IsOffTheRecord()}),
      base::BindRepeating(
          [](content::GlobalRenderFrameHostId rfh_id)
              -> content::BrowserContext* {
            auto* rfh = content::RenderFrameHost::FromID(rfh_id);
            return rfh ? rfh->GetBrowserContext() : nullptr;
          },
          rfh->GetGlobalId()),
      *rfh);

  ui::ClipboardMetadata metadata;
  metadata.format_type = ui::ClipboardFormatType::PngType();
  metadata.size = size.value_or(kShareThumbnailMaxSize);

  bool copy_check_required =
      enterprise_data_protection::IsCopyPolicyCheckRequired(source, metadata);

  ui::DataTransferEndpoint dte(glic::GetGuestURL());
  content::ClipboardEndpoint paste_destination(
      dte, base::BindRepeating(
               [](GlicKeyedService* service) -> content::BrowserContext* {
                 return service->profile();
               },
               base::Unretained(&service_.get())));

  bool paste_check_required =
      enterprise_data_protection::IsPastePolicyCheckRequired(
          source, paste_destination, metadata);

  return copy_check_required || paste_check_required;
}

void GlicShareImageHandler::MaybeStartObservingNavigation(
    tabs::TabInterface* tab) {
  if (!AreClipboardPolicyChecksRequired(std::nullopt)) {
    return;
  }

  // Listen for navigations and WebContents destruction.
  Observe(tab->GetContents());

  // Listen for WebContents discards.
  will_discard_web_contents_subscription_ = tab->RegisterWillDiscardContents(
      base::BindRepeating(&GlicShareImageHandler::OnWillDiscardContents,
                          base::Unretained(this)));

  // Listen for tab detachment.
  will_detach_subscription_ = tab->RegisterWillDetach(base::BindRepeating(
      &GlicShareImageHandler::OnWillDetach, base::Unretained(this)));
}

bool GlicShareImageHandler::MaybeStartObservingEligibility(
    tabs::TabInterface* tab) {
  if (!tab) {
    return false;
  }
  auto* helper = tabs::PageContextEligibilityHelper::From(tab);
  if (!helper) {
    return false;
  }
  eligibility_subscription_ =
      helper->RegisterEligibilityChangeCallback(base::BindRepeating(
          &GlicShareImageHandler::OnPageContextEligibilityChanged,
          weak_ptr_factory_.GetWeakPtr()));

  return helper->IsPageContextEligible() ==
         optimization_guide::PageContextEligibilityStatus::kEligible;
}

void GlicShareImageHandler::OnPageContextEligibilityChanged(
    optimization_guide::PageContextEligibilityStatus eligibility) {
  if (eligibility !=
      optimization_guide::PageContextEligibilityStatus::kEligible) {
    ShareComplete(ShareImageResult::kFailedNoTabContext);
    return;
  }
}

void GlicShareImageHandler::Reset() {
  // TODO(b:461529494): Put this state in a struct.
  chrome_render_frame_remote_.reset();
  eligibility_subscription_ = base::CallbackListSubscription();
  tab_handle_ = tabs::TabHandle::Null();
  render_frame_host_id_ = content::GlobalRenderFrameHostId();
  src_url_ = GURL();
  frame_url_ = GURL();
  frame_origin_ = url::Origin();
  StopObservingNavigation();

  // Ensure that async callbacks aren't invoked.
  weak_ptr_factory_.InvalidateWeakPtrs();

  is_share_in_progress_ = false;
}

}  // namespace glic
