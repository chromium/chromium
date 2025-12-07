// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_share_image_handler.h"

#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/data_protection/data_protection_clipboard_utils.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/clipboard/clipboard_format_type.h"
#include "ui/base/clipboard/clipboard_metadata.h"
#include "ui/base/data_transfer_policy/data_transfer_endpoint.h"

namespace glic {

namespace {

content::RenderFrameHost* GetGuestFrame(
    content::RenderFrameHost* parent_frame) {
  if (!parent_frame) {
    return nullptr;
  }
  content::RenderFrameHost* guest_frame = nullptr;
  parent_frame->ForEachRenderFrameHostWithAction(
      [&guest_frame](content::RenderFrameHost* rfh) {
        if (rfh->GetLastCommittedOrigin() == GetGuestOrigin()) {
          guest_frame = rfh;
          return content::RenderFrameHost::FrameIterationAction::kStop;
        }
        return content::RenderFrameHost::FrameIterationAction::kContinue;
      });
  return guest_frame;
}

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
                                  weak_ptr_factory_.GetWeakPtr()));
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
  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
  if (!browser) {
    ShareComplete(ShareImageResult::kFailedNoBrowser);
    return;
  }

  auto* instance = service_->GetInstanceForTab(tab);
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    if (instance &&
        instance->GetPanelState().kind == mojom::PanelStateKind::kDetached) {
      CHECK(instance->IsShowing()) << ", should be showing if detached";
      service_->CloseFloatingPanel();
    }
    // We always want to call ToggleUI for multi-instance to force a new
    // instance to be created.
    glic_panel_open_time_ = base::TimeTicks::Now();
    // Note: if the FRE was showing, this will just cause it to be reshown.
    service_->ToggleUI(browser, /*prevent_close=*/true,
                       mojom::InvocationSource::kSharedImage);
  } else if (!instance || !instance->IsShowing()) {
    glic_panel_open_time_ = base::TimeTicks::Now();
    // Note: if the FRE was showing, this will just cause it to be reshown.
    service_->ToggleUI(browser, /*prevent_close=*/true,
                       mojom::InvocationSource::kSharedImage);
  }

  PerformPastePolicyCheckWhenReady();
}

void GlicShareImageHandler::PerformPastePolicyCheckWhenReady() {
  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab) {
    ShareComplete(ShareImageResult::kFailedNoTab);
  } else if (IsClientReady(*tab)) {
    glic_panel_ready_timer_.Stop();
    DoPastePolicyCheck();
  } else if (base::TimeTicks::Now() - glic_panel_open_time_ >
             kShareTimeoutSeconds) {
    ShareComplete(ShareImageResult::kFailedTimedOut);
  } else if (!glic_panel_ready_timer_.IsRunning()) {
    glic_panel_ready_timer_.Start(
        FROM_HERE, kGlicPanelPollIntervalMilliseconds,
        base::BindRepeating(
            &GlicShareImageHandler::PerformPastePolicyCheckWhenReady,
            base::Unretained(this)));
  }
}

void GlicShareImageHandler::DoPastePolicyCheck() {
  auto* tab = tab_handle_.Get();
  if (!tab) {
    ShareComplete(ShareImageResult::kFailedNoTab);
    return;
  }

  auto* instance = service_->GetInstanceForTab(tab);
  if (!instance) {
    ShareComplete(ShareImageResult::kFailedNoInstance);
    return;
  }

  auto* host = &instance->host();
  auto* glic_contents = host->webui_contents();
  auto* glic_rfh = GetGuestFrame(glic_contents->GetPrimaryMainFrame());
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

  if (!IsClientReady(*tab)) {
    ShareComplete(ShareImageResult::kFailedClientUnreadied);
  }

  ShareComplete(ShareImageResult::kSuccess);
}

bool GlicShareImageHandler::IsClientReady(tabs::TabInterface& tab) {
  if (GlicInstance* instance = service_->GetInstanceForTab(&tab)) {
    return instance->host().IsReady();
  }
  return false;
}

void GlicShareImageHandler::ShareComplete(ShareImageResult result) {
  if (result == ShareImageResult::kSuccess) {
    service_->SendAdditionalContext(tab_handle_,
                                    std::move(additional_context_));
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

  if (BrowserWindowInterface* browser = tab->GetBrowserWindowInterface()) {
    if (auto* controller = browser->GetFeatures().toast_controller()) {
      controller->MaybeShowToast(ToastParams(ToastId::kGlicShareImageFailed));
    }
  }
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

  // Ensure that async callbacks aren't invoked.
  weak_ptr_factory_.InvalidateWeakPtrs();

  is_share_in_progress_ = false;
}

}  // namespace glic
