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
#include "base/memory/ptr_util.h"
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

namespace pdf {

namespace {

// Static factory instance (always nullptr for non-test).
PdfViewerStreamManager::Factory* g_factory = nullptr;

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
    : internal_id_(embed_internal_id), stream_(std::move(stream_container)) {
  // Make sure 0 is never used because some APIs (particularly WebRequest) have
  // special meaning for 0 IDs.
  static int32_t next_instance_id = 0;
  instance_id_ = ++next_instance_id;
}

PdfViewerStreamManager::StreamInfo::~StreamInfo() = default;

void PdfViewerStreamManager::StreamInfo::SetExtensionNavigated() {
  CHECK(!did_extension_navigate_);
  did_extension_navigate_ = true;
}

bool PdfViewerStreamManager::StreamInfo::DidPdfContentNavigate() const {
  return container_manager_.is_bound();
}

PdfViewerStreamManager::PdfViewerStreamManager(content::WebContents* contents)
    : content::WebContentsObserver(contents),
      content::WebContentsUserData<PdfViewerStreamManager>(*contents) {}

PdfViewerStreamManager::~PdfViewerStreamManager() = default;

// static
void PdfViewerStreamManager::Create(content::WebContents* contents) {
  if (FromWebContents(contents)) {
    return;
  }

  if (g_factory) {
    g_factory->CreatePdfViewerStreamManager(contents);
  } else {
    // Using `new` to access a non-public constructor.
    contents->SetUserData(
        UserDataKey(), base::WrapUnique(new PdfViewerStreamManager(contents)));
  }
}

// static
PdfViewerStreamManager* PdfViewerStreamManager::FromRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  return FromWebContents(
      content::WebContents::FromRenderFrameHost(render_frame_host));
}

// static
void PdfViewerStreamManager::SetFactoryForTesting(Factory* factory) {
  if (factory) {
    CHECK(!g_factory);
  }
  g_factory = factory;
}

void PdfViewerStreamManager::AddStreamContainer(
    int frame_tree_node_id,
    const std::string& internal_id,
    std::unique_ptr<extensions::StreamContainer> stream_container) {
  CHECK(stream_container);

  // If an entry with the same frame tree node ID already exists in
  // `stream_infos_`, then a new PDF navigation has occurred. If the
  // existing `StreamInfo` hasn't been claimed, replace the entry. This is safe,
  // since `GetStreamContainer()` verifies the original PDF URL. If the existing
  // `StreamInfo` has been claimed, then it will eventually be deleted, and the
  // new `StreamInfo` will be used instead. This can occur if a full page PDF
  // viewer refreshes or navigates to another PDF URL.
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
      stream_info->stream()->original_url()) {
    return nullptr;
  }

  return stream_info->stream()->GetWeakPtr();
}

bool PdfViewerStreamManager::PluginCanSave(
    content::RenderFrameHost* embedder_host) {
  auto* stream_info = GetClaimedStreamInfo(embedder_host);
  if (!stream_info) {
    return false;
  }

  return stream_info->plugin_can_save();
}

void PdfViewerStreamManager::SetPluginCanSave(
    content::RenderFrameHost* embedder_host,
    bool plugin_can_save) {
  auto* stream_info = GetClaimedStreamInfo(embedder_host);
  if (!stream_info) {
    return;
  }

  stream_info->set_plugin_can_save(plugin_can_save);
}

void PdfViewerStreamManager::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  // When the PDF embeder frame is deleted, delete its stream.
  if (GetClaimedStreamInfo(render_frame_host)) {
    DeleteClaimedStreamInfo(render_frame_host);
    // DO NOT add code past this point. `this` may have been deleted.
    return;
  }

  // If `render_frame_host` isn't active, ignore. An unclaimed `StreamInfo`'s
  // FrameTreeNode may delete a speculative `content::RenderFrameHost` before
  // the embedder `content::RenderFrameHost` commits and claims the stream. The
  // speculative `content::RenderFrameHost` won't be considered active, and
  // shouldn't cause the stream to be deleted.
  if (!render_frame_host->IsActive()) {
    return;
  }

  // If `render_frame_host` is an unrelated host (there isn't an unclaimed
  // stream), ignore.
  int frame_tree_node_id = render_frame_host->GetFrameTreeNodeId();
  if (!ContainsUnclaimedStreamInfo(frame_tree_node_id)) {
    return;
  }

  DeleteUnclaimedStreamInfo(frame_tree_node_id);
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
  DeleteClaimedStreamInfo(old_host);
  // DO NOT add code past this point. `this` may have been deleted.
}

void PdfViewerStreamManager::FrameDeleted(int frame_tree_node_id) {
  // If an embedder host is deleted, delete the associated `StreamInfo`.
  for (auto iter = stream_infos_.begin(); iter != stream_infos_.end();) {
    if (iter->first.frame_tree_node_id == frame_tree_node_id) {
      StreamInfo* stream_info = iter->second.get();
      if (stream_info->mime_handler_view_container_manager()) {
        stream_info->mime_handler_view_container_manager()
            ->DestroyFrameContainer(stream_info->instance_id());
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
  // host. The `embedder_host` should claim the unclaimed `StreamInfo`. This
  // should replace any existing `StreamInfo` objects related to
  // `embedder_host`. This is safe since `GetStreamContainer()` checks the
  // original URL for URL spoofs, and any security-relevant changes in the
  // response should result in a different `content::RenderFrameHost`.
  content::RenderFrameHost* embedder_host =
      navigation_handle->GetRenderFrameHost();
  if (!ContainsUnclaimedStreamInfo(embedder_host->GetFrameTreeNodeId())) {
    return;
  }

  StreamInfo* claimed_stream_info = ClaimStreamInfo(embedder_host);

  // Set the internal ID to set up postMessage later, when the PDF content host
  // finishes navigating.
  auto container_manager = GetMimeHandlerViewContainerManager(embedder_host);
  container_manager->SetInternalId(claimed_stream_info->internal_id());
}

void PdfViewerStreamManager::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  // Maybe set up postMessage support after the PDF content host finishes
  // navigating.
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
  if (!stream_info || stream_info->did_extension_navigate()) {
    return;
  }

  content::NavigationController::LoadURLParams params(
      stream_info->stream()->handler_url());
  params.frame_tree_node_id = about_blank_host->GetFrameTreeNodeId();
  params.source_site_instance = embedder_host->GetSiteInstance();
  web_contents()->GetController().LoadURLWithParams(params);

  stream_info->SetExtensionNavigated();
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

PdfViewerStreamManager::StreamInfo*
PdfViewerStreamManager::GetClaimedStreamInfoFromPdfContentNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsPdf()) {
    return nullptr;
  }

  // `navigation_handle` is for a PDF content frame, as checked by
  // `NavigationHandle::IsPdf()`.
  content::RenderFrameHost* embedder_host =
      GetEmbedderHostFromPdfContentNavigation(navigation_handle);
  CHECK(embedder_host);

  return GetClaimedStreamInfo(embedder_host);
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

void PdfViewerStreamManager::DeleteClaimedStreamInfo(
    content::RenderFrameHost* embedder_host) {
  auto iter = stream_infos_.find(GetEmbedderHostInfo(embedder_host));
  CHECK(iter != stream_infos_.end());

  StreamInfo* stream_info = iter->second.get();
  if (stream_info->mime_handler_view_container_manager()) {
    stream_info->mime_handler_view_container_manager()->DestroyFrameContainer(
        stream_info->instance_id());
  }

  stream_infos_.erase(iter);

  if (stream_infos_.empty()) {
    web_contents()->RemoveUserData(UserDataKey());
    // DO NOT add code past this point. RemoveUserData() deleted `this`.
  }
}

void PdfViewerStreamManager::DeleteUnclaimedStreamInfo(int frame_tree_node_id) {
  CHECK(stream_infos_.erase(GetUnclaimedEmbedderHostInfo(frame_tree_node_id)));

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
  auto* claimed_stream_info =
      GetClaimedStreamInfoFromPdfContentNavigation(navigation_handle);
  if (!claimed_stream_info) {
    return false;
  }

  navigation_handle->RegisterSubresourceOverride(
      claimed_stream_info->stream()->TakeTransferrableURLLoader());

  return true;
}

bool PdfViewerStreamManager::MaybeSetUpPostMessage(
    content::NavigationHandle* navigation_handle) {
  // Only set up postMessage if `navigation_handle` is for the PDF content
  // frame.
  auto* claimed_stream_info =
      GetClaimedStreamInfoFromPdfContentNavigation(navigation_handle);
  if (!claimed_stream_info) {
    return false;
  }

  // `navigation_handle` is for a PDF content frame, as checked by
  // `NavigationHandle::IsPdf()`.
  content::RenderFrameHost* embedder_host =
      GetEmbedderHostFromPdfContentNavigation(navigation_handle);
  CHECK(embedder_host);

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
  container_manager->DidLoad(claimed_stream_info->instance_id(),
                             claimed_stream_info->stream()->original_url());
  claimed_stream_info->set_mime_handler_view_container_manager(
      std::move(container_manager));

  return true;
}

void PdfViewerStreamManager::SetUpBeforeUnloadControl(
    mojo::PendingRemote<extensions::mime_handler::BeforeUnloadControl>
        before_unload_control_remote) {
  // TODO(crbug.com/1445746): Currently a no-op. Support the beforeunload API.
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(PdfViewerStreamManager);

}  // namespace pdf
