// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/context/glic_share_image_handler.h"

#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/glic/fre/glic_fre_controller.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "content/public/browser/render_frame_host.h"

namespace glic {

namespace {
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
    tabs::TabHandle handle) {
  // TODO(b:448726704): update to use an Image part.
  auto context = glic::mojom::AdditionalContext::New();
  std::vector<glic::mojom::AdditionalContextPartPtr> parts;
  auto context_data = mojom::ContextData::New();
  context_data->mime_type = "image/png";
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

  // Since we have no share in progress, we should not be waiting for the panel
  // to be ready.
  CHECK(!glic_panel_ready_timer_.IsRunning());

  Reset();
  is_share_in_progress_ = true;
  service_->metrics()->OnShareImageStarted();

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
      base::BindOnce(&GlicShareImageHandler::ShareCapturedImage,
                     base::Unretained(this), tab->GetHandle(), src_url,
                     render_frame_host->GetLastCommittedURL(),
                     render_frame_host->GetLastCommittedOrigin()));
}

void GlicShareImageHandler::ShareCapturedImage(
    tabs::TabHandle tab_handle,
    const GURL& src_url,
    const GURL& frame_url,
    const url::Origin& frame_origin,
    const std::vector<uint8_t>& thumbnail_data,
    const gfx::Size& original_size,
    const gfx::Size& downscaled_size,
    const std::string& image_extension,
    std::vector<lens::mojom::LatencyLogPtr> log_data) {
  // Close the remote since we've received our thumbnail.
  chrome_render_frame_remote_.reset();

  tab_handle_ = tab_handle;
  if (thumbnail_data.empty()) {
    ShareComplete(ShareImageResult::kFailedNoImage);
    return;
  }

  tabs::TabInterface* tab = tab_handle.Get();
  if (!tab) {
    ShareComplete(ShareImageResult::kFailedNoTab);
    return;
  }

  BrowserWindowInterface* browser = tab->GetBrowserWindowInterface();
  if (!browser) {
    ShareComplete(ShareImageResult::kFailedNoBrowser);
    return;
  }

  additional_context_ = CreateAdditionalContext(
      src_url, frame_url, frame_origin,
      base::span<const uint8_t>(thumbnail_data), tab_handle);

  auto* instance = service_->GetInstanceForTab(tab);
  if (!instance || !instance->IsShowing()) {
    glic_panel_open_time_ = base::TimeTicks::Now();
    // Note: if the FRE was showing, this will just cause it to be reshown.
    service_->ToggleUI(browser, /*prevent_close=*/true,
                       mojom::InvocationSource::kSharedImage);
  }

  SendAdditionalContextWhenReady();
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
  } else {
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

void GlicShareImageHandler::SendAdditionalContextWhenReady() {
  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab) {
    ShareComplete(ShareImageResult::kFailedNoTab);
  } else if (IsClientReady(*tab)) {
    ShareComplete(ShareImageResult::kSuccess);
  } else if (base::TimeTicks::Now() - glic_panel_open_time_ >
             kShareTimeoutSeconds) {
    ShareComplete(ShareImageResult::kFailedTimedOut);
  } else if (!glic_panel_ready_timer_.IsRunning()) {
    glic_panel_ready_timer_.Start(
        FROM_HERE, kGlicPanelPollIntervalMilliseconds,
        base::BindRepeating(
            &GlicShareImageHandler::SendAdditionalContextWhenReady,
            base::Unretained(this)));
  }
}

void GlicShareImageHandler::Reset() {
  glic_panel_open_time_ = base::TimeTicks();
  glic_panel_ready_timer_.Stop();
  additional_context_.reset();
  chrome_render_frame_remote_.reset();
  tab_handle_ = tabs::TabHandle::Null();
  is_share_in_progress_ = false;
}

}  // namespace glic
