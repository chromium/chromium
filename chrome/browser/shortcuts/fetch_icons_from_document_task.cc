// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shortcuts/fetch_icons_from_document_task.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ref.h"
#include "base/types/expected.h"
#include "components/webapps/common/web_page_metadata.mojom.h"
#include "components/webapps/common/web_page_metadata_agent.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/favicon/favicon_url.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"

namespace shortcuts {

FetchIconsFromDocumentTask::FetchIconsFromDocumentTask(
    base::PassKey<DocumentIconFetcher>,
    content::RenderFrameHost& rfh)
    : frame_host_(rfh) {
  CHECK(frame_host_->IsInPrimaryMainFrame());
}

FetchIconsFromDocumentTask::~FetchIconsFromDocumentTask() = default;

void FetchIconsFromDocumentTask::Start(
    FetchIconsFromDocumentCallback callback) {
  callback_ = std::move(callback);
  mojo::AssociatedRemote<webapps::mojom::WebPageMetadataAgent> metadata_agent;
  frame_host_->GetRemoteAssociatedInterfaces()->GetInterface(&metadata_agent);

  // Set the error handler so that we can run abort this task if the WebContents
  // or the RenderFrameHost are destroyed and the connection to
  // ChromeRenderFrame is lost.
  metadata_agent.set_disconnect_handler(
      base::BindOnce(&FetchIconsFromDocumentTask::OnMetadataFetchError,
                     weak_factory_.GetWeakPtr()));

  // Bind the InterfacePtr into the callback so that it's kept alive
  // until there's either a connection error or a response.
  auto* web_page_metadata_proxy = metadata_agent.get();
  web_page_metadata_proxy->GetWebPageMetadata(
      base::BindOnce(&FetchIconsFromDocumentTask::OnWebPageMetadataObtained,
                     weak_factory_.GetWeakPtr(), std::move(metadata_agent)));
}

FetchIconsFromDocumentCallback FetchIconsFromDocumentTask::TakeCallback() {
  return std::move(callback_);
}

void FetchIconsFromDocumentTask::OnWebPageMetadataObtained(
    mojo::AssociatedRemote<webapps::mojom::WebPageMetadataAgent> metadata_agent,
    webapps::mojom::WebPageMetadataPtr web_page_metadata) {
  metadata_fetch_complete_ = true;
  std::vector<GURL> icons;
  for (const auto& icon_info : web_page_metadata->icons) {
    icons.push_back(icon_info->url);
  }
  for (const auto& favicon_url : frame_host_->FaviconURLs()) {
    icons.push_back(favicon_url->icon_url);
  }

  // Eliminate duplicates.
  base::flat_set<GURL> icon_set(std::move(icons));
  num_pending_image_requests_ = icon_set.size();

  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(&frame_host_.get());
  for (const GURL& url : icon_set) {
    web_contents->DownloadImageInFrame(
        frame_host_->GetGlobalId(), url, /*is_favicon=*/true,
        /*preferred_size=*/gfx::Size(),
        /*max_bitmap_size=*/0, /*bypass_cache=*/false,
        base::BindOnce(&FetchIconsFromDocumentTask::DidDownloadFavicon,
                       weak_factory_.GetWeakPtr()));
  }
  MaybeCompleteImageDownloadAndSelfDestruct();
}

void FetchIconsFromDocumentTask::DidDownloadFavicon(
    int id,
    int http_status_code,
    const GURL& image_url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& sizes) {
  icons_.reserve(icons_.size() + bitmaps.size());
  for (const SkBitmap& bitmap : bitmaps) {
    if (bitmap.drawsNothing()) {
      continue;
    }
    icons_.push_back(bitmap);
  }
  --num_pending_image_requests_;
  MaybeCompleteImageDownloadAndSelfDestruct();
}

void FetchIconsFromDocumentTask::MaybeCompleteImageDownloadAndSelfDestruct() {
  if (!metadata_fetch_complete_ || num_pending_image_requests_ > 0) {
    return;
  }
  OnCompleteSelfDestruct(base::ok(std::move(icons_)));
}

void FetchIconsFromDocumentTask::OnCompleteSelfDestruct(Result result,
                                                        base::Location here) {
  if (!callback_) {
    return;
  }
  std::move(callback_).Run(std::move(result));
}

void FetchIconsFromDocumentTask::OnMetadataFetchError() {
  OnCompleteSelfDestruct(
      base::unexpected(FetchIconsForDocumentError::kMetadataFetchFailed));
}

}  // namespace shortcuts
