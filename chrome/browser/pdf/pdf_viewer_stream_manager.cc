// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/pdf/pdf_viewer_stream_manager.h"

#include <stdint.h>

#include <memory>
#include <tuple>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/browser/guest_view/mime_handler_view/mime_handler_view_guest.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "url/gurl.h"

namespace pdf {

namespace {

const char kPdfExtensionUrl[] =
    "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/index.html";

// Creates a claimed `EmbedderHostInfo` from the `embedder_host`.
PdfViewerStreamManager::EmbedderHostInfo GetEmbedderHostInfo(
    content::RenderFrameHost* embedder_host) {
  return {embedder_host->GetFrameTreeNodeId(), embedder_host->GetGlobalId()};
}

// Creates a new unclaimed `EmbedderHostInfo` for the given frame tree node ID
// (without the `content::GlobalRenderFrameHostId`).
PdfViewerStreamManager::EmbedderHostInfo GetUnclaimedEmbedderHostInfo(
    int frame_tree_node_id) {
  return {frame_tree_node_id, content::GlobalRenderFrameHostId()};
}

// Gets the embedder host from the PDF content host's navigation handle.
content::RenderFrameHost* GetEmbedderHostFromPdfContentNavigation(
    content::NavigationHandle* navigation_handle) {
  // Since `navigation_handle` is for a PDF content frame, the parent frame is
  // the PDF extension frame, and the grandparent frame is the embedder frame.
  content::RenderFrameHost* extension_host =
      navigation_handle->GetParentFrame();
  CHECK(extension_host);

  return extension_host->GetParent();
}

// Gets the `extensions::mojom::MimeHandlerViewContainerManager` from the
// `container_host`.
mojo::AssociatedRemote<extensions::mojom::MimeHandlerViewContainerManager>
GetMimeHandlerViewContainerManager(content::RenderFrameHost* container_host) {
  CHECK(container_host);

  mojo::AssociatedRemote<extensions::mojom::MimeHandlerViewContainerManager>
      container_manager;
  container_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &container_manager);
  return container_manager;
}

}  // namespace

bool PdfViewerStreamManager::EmbedderHostInfo::operator<(
    const PdfViewerStreamManager::EmbedderHostInfo& other) const {
  return std::tie(frame_tree_node_id, global_id) <
         std::tie(other.frame_tree_node_id, other.global_id);
}

PdfViewerStreamManager::StreamInfo::StreamInfo(
    const std::string& embed_internal_id,
    std::unique_ptr<extensions::StreamContainer> stream_container)
    : internal_id(embed_internal_id), stream(std::move(stream_container)) {
  // Make sure 0 is never used because some APIs (particularly WebRequest) have
  // special meaning for 0 IDs.
  static int32_t next_instance_id = 0;
  instance_id = ++next_instance_id;
}

PdfViewerStreamManager::StreamInfo::~StreamInfo() = default;

PdfViewerStreamManager::PdfViewerStreamManager(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<PdfViewerStreamManager>(*contents) {}

PdfViewerStreamManager::~PdfViewerStreamManager() = default;

void PdfViewerStreamManager::AddStreamContainer(
    int frame_tree_node_id,
    const std::string& internal_id,
    std::unique_ptr<extensions::StreamContainer> stream_container) {
  CHECK(stream_container);

  // If an entry with the same frame tree node ID already exists in
  // `stream_infos_`, then a new PDF navigation has occurred. If the
  // existing `StreamInfo` hasn't been claimed, replace the entry. This is safe,
  // since `GetStreamContainer()` verifies the original PDF URL. If the existing
  // `StreamInfo` has been claimed, and the embedder host is replaced, then the
  // original `StreamInfo` will eventually be deleted, and the new `StreamInfo`
  // will be used instead.
  auto embedder_host_info = GetUnclaimedEmbedderHostInfo(frame_tree_node_id);
  stream_infos_[embedder_host_info] =
      std::make_unique<StreamInfo>(internal_id, std::move(stream_container));
}

base::WeakPtr<extensions::StreamContainer>
PdfViewerStreamManager::GetStreamContainer(
    content::RenderFrameHost* embedder_host) {
  auto* stream_info = GetClaimedStreamInfo(embedder_host);
  if (!stream_info) {
    return nullptr;
  }

  // It's possible to have multiple `extensions::StreamContainer`s under the
  // same frame tree node ID. Verify the original URL in the stream container to
  // avoid a potential URL spoof.
  if (embedder_host->GetLastCommittedURL() !=
      stream_info->stream->original_url()) {
    return nullptr;
  }

  return stream_info->stream->GetWeakPtr();
}

void PdfViewerStreamManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  // If this is an unrelated host, ignore.
  StreamInfo* claimed_stream_info = GetClaimedStreamInfo(render_frame_host);
  if (!claimed_stream_info &&
      !ContainsUnclaimedStreamInfo(render_frame_host->GetFrameTreeNodeId())) {
    return;
  }

  // An unclaimed `StreamInfo`'s FrameTreeNode may delete a speculative
  // `content::RenderFrameHost` before the embedder `content::RenderFrameHost`
  // commits and claims the stream. The speculative `content::RenderFrameHost`
  // won't be considered active, and shouldn't cause the stream to be deleted.
  if (!claimed_stream_info && !render_frame_host->IsActive()) {
    return;
  }

  DeleteStreamInfo(render_frame_host);
  // DO NOT add code past this point. `this` may have been deleted.
}

void PdfViewerStreamManager::RenderFrameHostChanged(
    content::RenderFrameHost* old_host,
    content::RenderFrameHost* new_host) {
  // If the `old_host` is null, then it means that a subframe is being created.
  // Don't treat this like a host change.
  if (!old_host) {
    return;
  }

  // If this is an unrelated host, ignore.
  if (!GetClaimedStreamInfo(old_host)) {
    return;
  }

  // The `old_host`'s `StreamInfo` should be deleted since this event could be
  // triggered from navigating the embedder host to a non-PDF URL. If the
  // embedder host is navigating to another PDF URL, then a new `StreamInfo`
  // should have already been created and claimed by `new_host`, so it's still
  // safe to delete `old_host`'s `StreamInfo`.
  DeleteStreamInfo(old_host);
}

void PdfViewerStreamManager::FrameDeleted(int frame_tree_node_id) {
  // If an embedder host is deleted, delete the associated `StreamInfo`.
  for (auto iter = stream_infos_.begin(); iter != stream_infos_.end();) {
    if (iter->first.frame_tree_node_id == frame_tree_node_id) {
      StreamInfo* stream_info = iter->second.get();
      if (stream_info->container_manager) {
        stream_info->container_manager->DestroyFrameContainer(
            stream_info->instance_id);
      }

      iter = stream_infos_.erase(iter);
    } else {
      ++iter;
    }
  }

  // Delete `this` if there are no remaining stream infos.
  if (stream_infos_.empty()) {
    web_contents()->RemoveUserData(UserDataKey());
    // DO NOT add code past this point. RemoveUserData() deleted `this`.
  }
}

void PdfViewerStreamManager::ReadyToCommitNavigation(
    content::NavigationHandle* navigation_handle) {
  // Maybe register a PDF subresource override in the PDF content host.
  if (MaybeRegisterPdfSubresourceOverride(navigation_handle)) {
    return;
  }

  // The initial load notification for the URL being served in the embedder
  // host. If there isn't already an existing claimed `StreamInfo`, then
  // `embedder_host` should claim the unclaimed `StreamInfo`.
  content::RenderFrameHost* embedder_host =
      navigation_handle->GetRenderFrameHost();
  if (!GetClaimedStreamInfo(embedder_host) &&
      !ContainsUnclaimedStreamInfo(embedder_host->GetFrameTreeNodeId())) {
    return;
  }

  StreamInfo* claimed_stream_info = ClaimStreamInfo(embedder_host);

  // Set the internal ID to set up postMessage later, when the PDF content host
  // finishes navigating.
  auto container_manager = GetMimeHandlerViewContainerManager(embedder_host);
  container_manager->SetInternalId(claimed_stream_info->internal_id);
}

void PdfViewerStreamManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Maybe set up postMessage support after the PDF content host finishes
  // navigating, which would delete the associated `StreamInfo`.
  if (MaybeSetUpPostMessage(navigation_handle)) {
    return;
  }

  // During PDF navigation, in the embedder host, an about:blank embed is
  // inserted in a synthetic HTML document as a placeholder for the PDF
  // extension. Navigate the about:blank embed to the PDF extension URL to load
  // the PDF extension.
  if (!navigation_handle->GetURL().IsAboutBlank()) {
    return;
  }

  // Ignore any `content::RenderFrameHost`s that aren't the expected PDF
  // about:blank host. The parent frame should be the tracked embedder
  // frame.
  content::RenderFrameHost* about_blank_host =
      navigation_handle->GetRenderFrameHost();
  if (!about_blank_host) {
    return;
  }

  content::RenderFrameHost* embedder_host = about_blank_host->GetParent();
  if (!embedder_host) {
    return;
  }

  // The `StreamInfo` should already have been claimed. Ignore if the extension
  // host has already navigated, to avoid multiple about:blanks navigating to
  // the extension URL.
  auto* stream_info = GetClaimedStreamInfo(embedder_host);
  if (!stream_info || stream_info->did_extension_navigate) {
    return;
  }

  const GURL url(kPdfExtensionUrl);
  content::NavigationController::LoadURLParams params(url);
  params.frame_tree_node_id = about_blank_host->GetFrameTreeNodeId();
  params.source_site_instance = embedder_host->GetSiteInstance();
  web_contents()->GetController().LoadURLWithParams(params);

  stream_info->did_extension_navigate = true;
}

void PdfViewerStreamManager::ClaimStreamInfoForTesting(
    content::RenderFrameHost* embedder_host) {
  ClaimStreamInfo(embedder_host);
}

PdfViewerStreamManager::StreamInfo*
PdfViewerStreamManager::GetClaimedStreamInfo(
    content::RenderFrameHost* embedder_host) {
  auto iter = stream_infos_.find(GetEmbedderHostInfo(embedder_host));
  if (iter == stream_infos_.end()) {
    return nullptr;
  }

  return iter->second.get();
}

bool PdfViewerStreamManager::ContainsUnclaimedStreamInfo(
    int frame_tree_node_id) const {
  return base::Contains(stream_infos_,
                        GetUnclaimedEmbedderHostInfo(frame_tree_node_id));
}

PdfViewerStreamManager::StreamInfo* PdfViewerStreamManager::ClaimStreamInfo(
    content::RenderFrameHost* embedder_host) {
  auto unclaimed_embedder_info =
      GetUnclaimedEmbedderHostInfo(embedder_host->GetFrameTreeNodeId());
  auto iter = stream_infos_.find(unclaimed_embedder_info);
  CHECK(iter != stream_infos_.end());

  PdfViewerStreamManager::StreamInfo* stream_info = iter->second.get();

  auto claimed_embedder_info = GetEmbedderHostInfo(embedder_host);
  stream_infos_[claimed_embedder_info] = std::move(iter->second);
  stream_infos_.erase(iter);

  return stream_info;
}

void PdfViewerStreamManager::DeleteStreamInfo(
    content::RenderFrameHost* embedder_host) {
  auto iter = stream_infos_.find(GetEmbedderHostInfo(embedder_host));
  CHECK(iter != stream_infos_.end());

  StreamInfo* stream_info = iter->second.get();
  if (stream_info->container_manager) {
    stream_info->container_manager->DestroyFrameContainer(
        stream_info->instance_id);
  }

  stream_infos_.erase(iter);

  if (stream_infos_.empty()) {
    web_contents()->RemoveUserData(UserDataKey());
    // DO NOT add code past this point. RemoveUserData() deleted `this`.
  }
}

bool PdfViewerStreamManager::MaybeRegisterPdfSubresourceOverride(
    content::NavigationHandle* navigation_handle) {
  // Only register the subresource override if `navigation_handle` is for the
  // PDF content frame. Ignore all other navigations in different frames, such
  // as navigations in the embedder frame or PDF extension frame.
  if (!navigation_handle->IsPdf()) {
    return false;
  }

  // `navigation_handle` is for a PDF content frame, as checked by
  // `NavigationHandle::IsPdf()`.
  content::RenderFrameHost* embedder_host =
      GetEmbedderHostFromPdfContentNavigation(navigation_handle);
  CHECK(embedder_host);

  auto* claimed_stream_info = GetClaimedStreamInfo(embedder_host);
  if (!claimed_stream_info) {
    return false;
  }

  // The stream container is no longer needed after registering the subresource
  // override.
  navigation_handle->RegisterSubresourceOverride(
      claimed_stream_info->stream->TakeTransferrableURLLoader());

  return true;
}

bool PdfViewerStreamManager::MaybeSetUpPostMessage(
    content::NavigationHandle* navigation_handle) {
  // Only set up postMessage if `navigation_handle` is for the PDF content
  // frame.
  if (!navigation_handle->IsPdf()) {
    return false;
  }

  // `navigation_handle` is for a PDF content frame, as checked by
  // `NavigationHandle::IsPdf()`.
  content::RenderFrameHost* embedder_host =
      GetEmbedderHostFromPdfContentNavigation(navigation_handle);
  CHECK(embedder_host);

  auto* claimed_stream_info = GetClaimedStreamInfo(embedder_host);
  if (!claimed_stream_info) {
    return false;
  }

  // If `owner_type` is kEmbed or kObject, then the PDF is embedded onto another
  // HTML page. `container_host` should be the PDF embedder host's parent.
  // Otherwise, the PDF is full-page, in which `container_host` should be the
  // PDF embedder host itself.
  auto owner_type = embedder_host->GetFrameOwnerElementType();
  bool is_full_page = owner_type != blink::FrameOwnerElementType::kEmbed &&
                      owner_type != blink::FrameOwnerElementType::kObject;
  auto* container_host =
      is_full_page ? embedder_host : embedder_host->GetParent();
  CHECK(container_host);

  auto container_manager = GetMimeHandlerViewContainerManager(container_host);

  // Set up beforeunload support for full page PDF viewer, which will also help
  // set up postMessage support.
  if (is_full_page) {
    container_manager->CreateBeforeUnloadControl(
        base::BindOnce(&PdfViewerStreamManager::SetUpBeforeUnloadControl,
                       weak_factory_.GetWeakPtr()));
  }

  // Enable postMessage support.
  // The first parameter for DidLoad() is
  // mime_handler_view_guest_element_instance_id, which is used to identify and
  // delete `extensions::MimeHandlerViewFrameContainer` objects. However, OOPIF
  // PDF viewer doesn't have a guest element instance ID. Use the instance ID
  // instead, which is a unique ID for `StreamInfo`.
  container_manager->DidLoad(claimed_stream_info->instance_id,
                             claimed_stream_info->stream->original_url());
  claimed_stream_info->container_manager = std::move(container_manager);

  return true;
}

void PdfViewerStreamManager::SetUpBeforeUnloadControl(
    mojo::PendingRemote<extensions::mime_handler::BeforeUnloadControl>
        before_unload_control_remote) {
  // TODO(crbug.com/1445746): Currently a no-op. Support the beforeunload API.
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PdfViewerStreamManager);

}  // namespace pdf
