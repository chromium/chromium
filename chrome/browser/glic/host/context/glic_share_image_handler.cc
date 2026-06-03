// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_share_image_handler.h"

#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"
#include "chrome/browser/glic/common/future_browser_features.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/chrome_features.h"
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

// Based on URLToImageMarkup from clipboard_utilities.cc.
std::u16string GetImageMarkup(const GURL& src_url,
                              content::RenderFrameHost* rfh) {
  if (!src_url.is_valid()) {
    return u"";
  }
  std::u16string alt = u"";
  auto* contents = content::WebContents::FromRenderFrameHost(rfh);
  if (contents) {
    std::u16string title = base::EscapeForHTML(contents->GetTitle());
    if (!title.empty()) {
      alt = base::StrCat({u" alt=\"", title, u"\""});
    }
  }
  std::u16string spec = base::EscapeForHTML(base::UTF8ToUTF16(src_url.spec()));
  return base::StrCat({u"<img src=\"", spec, u"\"", alt, u"></img>"});
}

constexpr int kShareThumbnailMinSize = 500 * 500;
constexpr int kShareThumbnailMaxWidth = 1000;
constexpr int kShareThumbnailMaxHeight = 1000;
constexpr base::TimeDelta kShareTimeoutSeconds = base::Seconds(60);
constexpr base::TimeDelta kGlicPanelPollIntervalMilliseconds =
    base::Milliseconds(60);

mojom::AdditionalContextPtr CreateAdditionalContext(
    const GURL& src_url,
    const GURL& frame_url,
    const url::Origin& frame_origin,
    base::span<const uint8_t> thumbnail_data,
    tabs::TabHandle handle,
    const std::string& mime_type,
    mojom::TabContextPtr tab_context) {
  // TODO(b:448726704): update to use an Image part.
  auto context = glic::mojom::AdditionalContext::New();
  std::vector<glic::mojom::AdditionalContextPartPtr> parts;
  auto context_data = mojom::ContextData::New();
  context->source = glic::mojom::AdditionalContextSource::kShareContextMenu;
  context_data->mime_type = mime_type;
  context_data->data = mojo_base::BigBuffer(thumbnail_data);
  parts.push_back(
      mojom::AdditionalContextPart::NewData(std::move(context_data)));
  parts.push_back(
      mojom::AdditionalContextPart::NewTabContext(std::move(tab_context)));
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

  // Since we have no share in progress, we should not be waiting for the panel
  // to be ready.
  CHECK(!glic_panel_ready_timer_.IsRunning());

  Reset();
  is_share_in_progress_ = true;
  service_->metrics()->OnShareImageStarted();

  tab_handle_ = tab->GetHandle();
  src_url_ = src_url;
  frame_url_ = render_frame_host->GetLastCommittedURL();
  frame_origin_ = render_frame_host->GetLastCommittedOrigin();
  render_frame_host_id_ = render_frame_host->GetGlobalId();

  // Listen for navigations and WebContents destruction.
  Observe(tab->GetContents());

  // Listen for WebContents discards.
  will_discard_web_contents_subscription_ = tab->RegisterWillDiscardContents(
      base::BindRepeating(&GlicShareImageHandler::OnWillDiscardContents,
                          base::Unretained(this)));

  // Listen for tab detachment.
  will_detach_subscription_ = tab->RegisterWillDetach(base::BindRepeating(
      &GlicShareImageHandler::OnWillDetach, base::Unretained(this)));

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
  if (!GlicEnabling::HasConsentedForProfile(service_->profile())) {
    ShareComplete(
        ShareImageResult::kFailedSawNavigationDidNotCompleteOnboarding);
  } else {
    ShareComplete(ShareImageResult::kFailedSawNavigation);
  }
}

void GlicShareImageHandler::OnInstanceWillBeDestroyed(GlicInstance* instance) {
  if (!instance_change_permitted_) {
    ShareComplete(ShareImageResult::kFailedLostInstance);
  }
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

  if (thumbnail_data.empty()) {
    ShareComplete(ShareImageResult::kFailedNoImage);
    return;
  }

  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab) {
    ShareComplete(ShareImageResult::kFailedNoTab);
    return;
  }

  mime_type_ = mime_type;
  thumbnail_data_ = thumbnail_data;

  auto options = mojom::GetTabContextOptions::New();
  // Ensure we don't have a huge number; matches actor_keyed_service.cc.
  options->max_meta_tags = 32;
  options->include_annotated_page_content = true;

  FetchPageContext(tab, *options,
                   base::BindOnce(&GlicShareImageHandler::OnReceivedTabContext,
                                  weak_ptr_factory_.GetWeakPtr()),
                   /*progress_listener=*/nullptr,
                   /*is_screenshot_annotated=*/false);
}

void GlicShareImageHandler::OnReceivedTabContext(
    base::expected<glic::mojom::GetContextResultPtr,
                   page_content_annotations::FetchPageContextErrorDetails>
        result) {
  if (!result.has_value() || !result.value()->is_tab_context()) {
    ShareComplete(ShareImageResult::kFailedNoTabContext);
    return;
  }

  auto* render_frame_host =
      content::RenderFrameHost::FromID(render_frame_host_id_);
  if (!render_frame_host) {
    ShareComplete(ShareImageResult::kFailedNoFrame);
    return;
  }

  additional_context_ = CreateAdditionalContext(
      src_url_, frame_url_, frame_origin_, thumbnail_data_, tab_handle_,
      mime_type_, std::move(result.value()->get_tab_context()));

  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab) {
    ShareComplete(ShareImageResult::kFailedNoTab);
    return;
  }

  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_frame_host_id_);
  if (!rfh) {
    ShareComplete(ShareImageResult::kFailedNoFrame);
    return;
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
  metadata.size = thumbnail_data_.size();

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

  bool do_policy_checks = copy_check_required || paste_check_required;
  PolicyCheck policy_check =
      do_policy_checks ? PolicyCheck::kClipboard : PolicyCheck::kNone;

  if (base::FeatureList::IsEnabled(features::kGlicShareImageViaInvoke)) {
    GlicInvokeOptions invoke_options(mojom::InvocationSource::kSharedImage);
    invoke_options.additional_context = AdditionalTabContext(
        std::move(additional_context_), render_frame_host_id_, policy_check);
    invoke_options.target.surface = tab;
    invoke_options.target.conversation = NewConversation();
    invoke_options.fre_override = mojom::FreOverride::kTrustFirstClick;
    invoke_options.on_error = base::BindOnce(
        &GlicShareImageHandler::OnInvokeError, weak_ptr_factory_.GetWeakPtr());
    invoke_options.on_success = base::BindOnce(
        &GlicShareImageHandler::ShareComplete, weak_ptr_factory_.GetWeakPtr(),
        ShareImageResult::kSentImageToClient);
    service_->Invoke(std::move(invoke_options));
    StopObservingNavigation();
    return;
  }

  if (!do_policy_checks) {
    StopObservingNavigation();
    if (OpenUI(tab)) {
      PerformTaskWhenReady(
          base::BindOnce(&GlicShareImageHandler::WaitForOnboardingCompletion,
                         weak_ptr_factory_.GetWeakPtr()));
    }
    return;
  }

  content::ClipboardPasteData data;
  data.png = thumbnail_data_;
  data.html = GetImageMarkup(src_url_, rfh);

  enterprise_data_protection::IsClipboardCopyAllowedByPolicy(
      source, metadata, data,
      base::BindOnce(&GlicShareImageHandler::OnCopyPolicyCheckComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GlicShareImageHandler::OnCopyPolicyCheckComplete(
    const ui::ClipboardFormatType& data_type,
    const content::ClipboardPasteData& data,
    std::optional<std::u16string> replacement_data) {
  if (replacement_data.has_value() || data.empty()) {
    ShareComplete(ShareImageResult::kFailedClipboardCopyPolicy);
    return;
  }

  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab) {
    ShareComplete(ShareImageResult::kFailedNoTab);
    return;
  }

  if (OpenUI(tab)) {
    PerformTaskWhenReady(
        base::BindOnce(&GlicShareImageHandler::DoPastePolicyCheck,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

void GlicShareImageHandler::PerformTaskWhenReady(base::OnceClosure callback) {
  on_client_ready_callback_ = std::move(callback);
  PerformTaskWhenReadyPolling();
}

void GlicShareImageHandler::PerformTaskWhenReadyPolling() {
  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab) {
    ShareComplete(ShareImageResult::kFailedNoTab);
    return;
  }

  std::optional<GlicInstance*> optional_instance = GetAndVerifyInstance(tab);
  std::optional<bool> optional_is_client_ready = IsClientReady(*tab);
  if (!optional_is_client_ready.has_value() || !optional_instance.has_value()) {
    // If we receive nullopt, then sharing has already been completed, so bail.
    return;
  }
  GlicInstance* instance = *optional_instance;

  if (*optional_is_client_ready) {
    glic_panel_ready_timer_.Stop();
    if (on_client_ready_callback_) {
      std::move(on_client_ready_callback_).Run();
    }
  } else if (base::TimeTicks::Now() - glic_panel_open_time_ >
             kShareTimeoutSeconds) {
    if (!instance) {
      ShareComplete(ShareImageResult::kFailedTimedOutNoInstance);
    } else if (!instance->host().IsWebClientConnected()) {
      ShareComplete(ShareImageResult::kFailedTimedOutNoWebClient);
    } else if (!GlicEnabling::HasConsentedForProfile(service_->profile())) {
      ShareComplete(ShareImageResult::kFailedTimedOutDidNotCompleteOnboarding);
    } else {
      ShareComplete(ShareImageResult::kFailedTimedOut);
    }
  } else if (!glic_panel_ready_timer_.IsRunning()) {
    // TODO(b/483387751): refactor to use invoke API.
    glic_panel_ready_timer_.Start(
        FROM_HERE, kGlicPanelPollIntervalMilliseconds,
        base::BindRepeating(&GlicShareImageHandler::PerformTaskWhenReadyPolling,
                            base::Unretained(this)));
  }
}

void GlicShareImageHandler::DoPastePolicyCheck() {
  auto* tab = tab_handle_.Get();
  if (!tab) {
    ShareComplete(ShareImageResult::kFailedNoTab);
    return;
  }

  std::optional<GlicInstance*> optional_instance = GetAndVerifyInstance(tab);
  if (!optional_instance) {
    // We have already called ShareComplete if we get nullopt.
    return;
  }

  GlicInstance* instance = *optional_instance;
  if (!instance) {
    ShareComplete(ShareImageResult::kFailedNoInstance);
    return;
  }

  // We base the paste policy check on the rfh we pull via the current instance.
  // Sharing will fail if the instance changes.
  // TODO(b/505428556): support retrying if the instance changes after this
  // point rather than failing.
  instance_change_permitted_ = false;

  auto* host = &instance->host();
  auto* glic_rfh = host->GetGuestMainFrame();
  if (!glic_rfh) {
    ShareComplete(ShareImageResult::kFailedNoFrame);
    return;
  }

  auto get_browser_context =
      [](content::GlobalRenderFrameHostId rfh_id) -> content::BrowserContext* {
    auto* rfh = content::RenderFrameHost::FromID(rfh_id);
    return rfh ? rfh->GetBrowserContext() : nullptr;
  };

  content::ClipboardEndpoint destination(
      ui::DataTransferEndpoint(
          glic_rfh->GetLastCommittedURL(),
          {.off_the_record = glic_rfh->GetBrowserContext()->IsOffTheRecord()}),
      base::BindRepeating(get_browser_context, glic_rfh->GetGlobalId()),
      *glic_rfh);

  auto* source_rfh = content::RenderFrameHost::FromID(render_frame_host_id_);
  if (!source_rfh) {
    ShareComplete(ShareImageResult::kFailedNoFrame);
    return;
  }

  content::ClipboardEndpoint source(
      ui::DataTransferEndpoint(
          source_rfh->GetMainFrame()->GetLastCommittedURL(),
          {.off_the_record =
               source_rfh->GetBrowserContext()->IsOffTheRecord()}),
      base::BindRepeating(get_browser_context, source_rfh->GetGlobalId()),
      *source_rfh);

  ui::ClipboardMetadata metadata;
  metadata.format_type = ui::ClipboardFormatType::PngType();
  metadata.size = thumbnail_data_.size();

  content::ClipboardPasteData paste_data;
  paste_data.png = thumbnail_data_;
  paste_data.html = GetImageMarkup(src_url_, source_rfh);

  enterprise_data_protection::PasteIfAllowedByPolicy(
      source, destination, metadata, std::move(paste_data),
      base::BindOnce(&GlicShareImageHandler::OnPastePolicyCheckComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void GlicShareImageHandler::OnPastePolicyCheckComplete(
    std::optional<content::ClipboardPasteData> data) {
  if (!data || data->png.empty()) {
    ShareComplete(ShareImageResult::kFailedClipboardPastePolicy);
    return;
  }
  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab) {
    ShareComplete(ShareImageResult::kFailedNoTab);
    return;
  }

  // At this point, we are no longer concerned with observing navigations or
  // WebContents destruction.
  StopObservingNavigation();

  WaitForOnboardingCompletion();
}

void GlicShareImageHandler::WaitForOnboardingCompletion() {
  if (GlicEnabling::HasConsentedForProfile(service_->profile())) {
    ShareComplete(ShareImageResult::kSentImageToClient);
    return;
  }

  onboarding_timeout_timer_.Start(
      FROM_HERE, base::Minutes(1),
      base::BindOnce(&GlicShareImageHandler::OnOnboardingTimeout,
                     weak_ptr_factory_.GetWeakPtr()));

  onboarding_subscription_ = service_->enabling().RegisterOnConsentChanged(
      base::BindRepeating(&GlicShareImageHandler::OnOnboardingStatusChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

void GlicShareImageHandler::OnOnboardingStatusChanged() {
  if (GlicEnabling::HasConsentedForProfile(service_->profile())) {
    onboarding_timeout_timer_.Stop();
    onboarding_subscription_ = base::CallbackListSubscription();
    ShareComplete(ShareImageResult::kSentImageToClient);
  }
}

void GlicShareImageHandler::OnOnboardingTimeout() {
  onboarding_subscription_ = base::CallbackListSubscription();
  ShareComplete(ShareImageResult::kFailedTimedOutDidNotCompleteOnboarding);
}

std::optional<bool> GlicShareImageHandler::IsClientReady(
    tabs::TabInterface& tab) {
  std::optional<GlicInstance*> optional_instance = GetAndVerifyInstance(&tab);
  if (!optional_instance) {
    return std::nullopt;
  }
  if (GlicInstance* instance = *optional_instance) {
    return instance->host().IsWebClientConnected();
  }
  return false;
}

std::optional<GlicInstance*> GlicShareImageHandler::GetAndVerifyInstance(
    tabs::TabInterface* tab) {
  GlicInstance* instance = service_->GetInstanceForTab(tab);
  InstanceId id = instance ? instance->id() : InstanceId::CreateNullId();
  if (id != instance_id_) {
    // TODO(b/501233062): add a browser test or migrate to the invoke API.
    if (!instance_change_permitted_) {
      if (is_share_in_progress_) {
        ShareComplete(ShareImageResult::kFailedLostInstance);
      }
      return std::nullopt;
    }
    instance_id_ = id;
    instance_destruction_subscription_ = {};
    if (instance) {
      instance_destruction_subscription_ = instance->RegisterWillBeDestroyed(
          base::BindOnce(&GlicShareImageHandler::OnInstanceWillBeDestroyed,
                         base::Unretained(this)));
    }
  }
  return instance;
}

void GlicShareImageHandler::OnInvokeError(GlicInvokeError error) {
  switch (error) {
    case GlicInvokeError::kUnknown:
      ShareComplete(ShareImageResult::kFailedUnknown);
      break;
    case GlicInvokeError::kTimeout:
      if (!GlicEnabling::HasConsentedForProfile(service_->profile())) {
        ShareComplete(
            ShareImageResult::kFailedTimedOutDidNotCompleteOnboarding);
      } else {
        ShareComplete(ShareImageResult::kFailedTimedOut);
      }
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
    default:
      ShareComplete(ShareImageResult::kFailedUnknown);
      break;
  }
}

void GlicShareImageHandler::ShareComplete(ShareImageResult result) {
  if (result == ShareImageResult::kSentImageToClient) {
    if (!base::FeatureList::IsEnabled(features::kGlicShareImageViaInvoke)) {
      // Do final checks for readiness before sending the context.
      tabs::TabInterface* tab = tab_handle_.Get();
      if (!tab) {
        ShareComplete(ShareImageResult::kFailedNoTab);
        return;
      }
      std::optional<bool> optional_is_client_ready = IsClientReady(*tab);
      if (!optional_is_client_ready.has_value()) {
        // If we get nullopt, it sharing is already completed, so bail.
        return;
      }
      bool is_client_ready = *optional_is_client_ready;
      if (!is_client_ready) {
        ShareComplete(ShareImageResult::kFailedClientUnreadied);
        return;
      }

      // If we're using the invoke API, then the context has already been sent.
      if (auto* instance = service_->GetInstanceForTab(tab)) {
        instance->SendAdditionalContext(std::move(additional_context_));
      }
    }
  } else if (result != ShareImageResult::kFailedClipboardPastePolicy &&
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

bool GlicShareImageHandler::OpenUI(tabs::TabInterface* tab) {
  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
  if (!browser) {
    ShareComplete(ShareImageResult::kFailedNoBrowser);
    return false;
  }

  // Changing the instance at this point is allowed, so we should not get
  // nullopt from this function.
  GlicInstance* instance = *GetAndVerifyInstance(tab);
  if (instance &&
      instance->GetPanelState().kind == mojom::PanelStateKind::kDetached) {
    CHECK(instance->IsShowing()) << ", should be showing if detached";
    service_->CloseFloatingPanel();
  }

  glic_panel_open_time_ = base::TimeTicks::Now();
  service_->ToggleUI(browser, /*prevent_close=*/true,
                     mojom::InvocationSource::kSharedImage);
  return true;
}

void GlicShareImageHandler::StopObservingNavigation() {
  // Ensure we're not observing any WebContents.
  Observe(nullptr);

  // Ensure we don't subscribe to discards of this WebContents.
  will_discard_web_contents_subscription_ = base::CallbackListSubscription();

  // Ensure we don't subscribe to tab detachment.
  will_detach_subscription_ = base::CallbackListSubscription();
}

void GlicShareImageHandler::Reset() {
  // TODO(b:461529494): Put this state in a struct.
  glic_panel_open_time_ = base::TimeTicks();
  glic_panel_ready_timer_.Stop();
  additional_context_.reset();
  chrome_render_frame_remote_.reset();
  tab_handle_ = tabs::TabHandle::Null();
  render_frame_host_id_ = content::GlobalRenderFrameHostId();
  src_url_ = GURL();
  frame_url_ = GURL();
  frame_origin_ = url::Origin();
  thumbnail_data_.clear();
  mime_type_ = "";
  StopObservingNavigation();
  instance_destruction_subscription_ = {};
  instance_id_ = InstanceId::CreateNullId();
  instance_change_permitted_ = true;
  onboarding_timeout_timer_.Stop();
  onboarding_subscription_ = base::CallbackListSubscription();

  on_client_ready_callback_.Reset();

  // Ensure that async callbacks aren't invoked.
  weak_ptr_factory_.InvalidateWeakPtrs();

  is_share_in_progress_ = false;
}

}  // namespace glic
