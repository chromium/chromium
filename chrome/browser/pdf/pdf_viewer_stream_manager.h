// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_PDF_VIEWER_STREAM_MANAGER_H_
#define CHROME_BROWSER_PDF_PDF_VIEWER_STREAM_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>

#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/common/mojom/guest_view.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace content {
struct GlobalRenderFrameHostId;
class NavigationHandle;
class WebContents;
}  // namespace content

namespace extensions {
namespace mime_handler {
class BeforeUnloadControl;
}
class StreamContainer;
}  // namespace extensions

namespace pdf {

// `PdfViewerStreamManager` is used for PDF navigation. It tracks all
// PDF navigation events in a `content::WebContents`. It handles multiple PDF
// viewer instances in a single `content::WebContents`. It is responsible for:
// 1. Storing the `extensions::StreamContainer` PDF data.
// 2. Observing for the PDF embedder RFH either navigating or closing (including
//    by crashing). This is necessary to ensure that streams that aren't claimed
//    are not leaked, by deleting the stream if any of those events occur.
// 3. Observing for the RFH created by the PDF embedder RFH to load the PDF
//    extension URL.
// 4. Observing for the PDF content RFH to register the stream as a subresource
//    override for the final PDF commit navigation and to set up postMessage
//    support.
// `PdfViewerStreamManager` is scoped to the `content::WebContents` it tracks,
// but it may also delete itself if all PDF streams are no longer used.
// `extensions::StreamContainer` objects are stored from
// `PluginResponseInterceptorURLLoaderThrottle::WillProcessResponse()` until
// the PDF viewer is no longer in use.
// Use `PdfViewerStreamManager::FromWebContents()` to get an instance.
class PdfViewerStreamManager
    : public content::WebContentsObserver,
      public content::WebContentsUserData<PdfViewerStreamManager> {
 public:
  // Information about the PDF embedder RFH needed to store and retrieve stream
  // containers.
  struct EmbedderHostInfo {
    // Need this comparator since this struct is used as a key in the
    // `stream_infos_` map.
    bool operator<(const EmbedderHostInfo& other) const;

    // Using the frame tree node ID to identify the embedder RFH is necessary
    // because entries are added during
    // `PluginResponseInterceptorURLLoaderThrottle::WillProcessResponse()`,
    // before the embedder's frame tree node has swapped from its previous RFH
    // to the embedder RFH that will hold the PDF.
    int frame_tree_node_id;
    content::GlobalRenderFrameHostId global_id;
  };

  PdfViewerStreamManager(const PdfViewerStreamManager&) = delete;
  PdfViewerStreamManager& operator=(const PdfViewerStreamManager&) = delete;
  ~PdfViewerStreamManager() override;

  // Returns a pointer to the `PdfViewerStreamManager` instance associated with
  // the `content::WebContents` of `render_frame_host`.
  static PdfViewerStreamManager* FromRenderFrameHost(
      content::RenderFrameHost* render_frame_host);

  // Starts tracking a `StreamContainer` in an embedder FrameTreeNode, before
  // the embedder host commits. The `StreamContainer` is considered unclaimed
  // until the embedder host commits, at which point the `StreamContainer` is
  // tracked by both the frame tree node ID and the render frame host ID.
  // Replaces existing unclaimed entries with the same `frame_tree_node_id`.
  // This can occur if an embedder frame navigating to a PDF starts navigating
  // to another PDF URL before the original `StreamContainer` is claimed.
  void AddStreamContainer(
      int frame_tree_node_id,
      const std::string& internal_id,
      std::unique_ptr<extensions::StreamContainer> stream_container);

  // Returns a pointer to a stream container that `embedder_host` has claimed or
  // nullptr if `embedder_host` hasn't claimed any stream containers.
  base::WeakPtr<extensions::StreamContainer> GetStreamContainer(
      content::RenderFrameHost* embedder_host);

  // Returns whether the PDF plugin should handle save events.
  bool PluginCanSave(content::RenderFrameHost* embedder_host);

  // Set whether the PDF plugin should handle save events.
  void SetPluginCanSave(content::RenderFrameHost* embedder_host,
                        bool plugin_can_save);

  // WebContentsObserver overrides.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void FrameDeleted(int frame_tree_node_id) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // For testing only. Mark an unclaimed stream info with the same frame tree
  // node ID as `embedder_host` as claimed by `embedder_host`. Callers must
  // ensure such a stream info exists before calling this.
  void ClaimStreamInfoForTesting(content::RenderFrameHost* embedder_host);

 protected:
  // Stream container stored for a single PDF navigation.
  class StreamInfo {
   public:
    StreamInfo(const std::string& embed_internal_id,
               std::unique_ptr<extensions::StreamContainer> stream_container);

    StreamInfo(const StreamInfo&) = delete;
    StreamInfo& operator=(const StreamInfo&) = delete;

    ~StreamInfo();

    const std::string& internal_id() const { return internal_id_; }

    extensions::StreamContainer* stream() { return stream_.get(); }

    bool did_extension_navigate() const { return did_extension_navigate_; }

    const mojo::AssociatedRemote<
        extensions::mojom::MimeHandlerViewContainerManager>&
    mime_handler_view_container_manager() const {
      return container_manager_;
    }

    void set_mime_handler_view_container_manager(
        mojo::AssociatedRemote<
            extensions::mojom::MimeHandlerViewContainerManager>
            container_manager) {
      container_manager_ = std::move(container_manager);
    }

    int32_t instance_id() const { return instance_id_; }

    void SetExtensionNavigated();

    bool DidPdfContentNavigate() const;

    bool plugin_can_save() const { return plugin_can_save_; }

    void set_plugin_can_save(bool plugin_can_save) {
      plugin_can_save_ = plugin_can_save;
    }

   private:
    // A unique ID for the PDF viewer instance. Used to set up postMessage
    // support for the full-page PDF viewer.
    const std::string internal_id_;

    // A container for the PDF stream. Holds data needed to load the PDF in the
    // PDF viewer.
    const std::unique_ptr<extensions::StreamContainer> stream_;

    // True if the extension host has navigated to the PDF extension URL. Used
    // to avoid navigating multiple about:blank child hosts to the PDF extension
    // URL.
    bool did_extension_navigate_ = false;

    // The container manager used to provide postMessage support.
    mojo::AssociatedRemote<extensions::mojom::MimeHandlerViewContainerManager>
        container_manager_;

    // A unique ID for this instance. Used for postMessage support to identify
    // `extensions::MimeHandlerViewFrameContainer` objects.
    int32_t instance_id_;

    // True if the PDF plugin should handle save events.
    bool plugin_can_save_ = false;
  };

  explicit PdfViewerStreamManager(content::WebContents* contents);

  // Returns the stream info claimed by `embedder_host`, or nullptr if there's
  // no existing stream.
  StreamInfo* GetClaimedStreamInfo(content::RenderFrameHost* embedder_host);

  // Returns the stream info for a PDF content navigation.
  StreamInfo* GetClaimedStreamInfoFromPdfContentNavigation(
      content::NavigationHandle* navigation_handle);

 private:
  FRIEND_TEST_ALL_PREFIXES(PdfViewerStreamManagerTest,
                           AddAndGetStreamContainer);

  friend class content::WebContentsUserData<PdfViewerStreamManager>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // Returns whether there's an unclaimed stream info with the default embedder
  // host info.
  bool ContainsUnclaimedStreamInfo(int frame_tree_node_id) const;

  // Mark an unclaimed stream info with the same frame tree node ID as
  // `embedder_host` as claimed by `embedder_host`. Returns a pointer to the
  // claimed stream info. Callers must ensure such a stream info exists with
  // `ContainsUnclaimedStreamInfo()` before calling this.
  StreamInfo* ClaimStreamInfo(content::RenderFrameHost* embedder_host);

  // Deletes the stream info associated with `embedder_host`, and deletes
  // `this` if there are no remaining stream infos.
  void DeleteStreamInfo(content::RenderFrameHost* embedder_host);

  // Intended to be called during the PDF content frame's
  // `ReadyToCommitNavigation()` event. Registers navigations occurring in a PDF
  // content frame as a subresource.
  bool MaybeRegisterPdfSubresourceOverride(
      content::NavigationHandle* navigation_handle);

  // Intended to be called during the PDF content frame's 'DidFinishNavigation'.
  // Sets up postMessage communication between the embedder frame and the PDF
  // extension frame after the PDF has finished loading.
  bool MaybeSetUpPostMessage(content::NavigationHandle* navigation_handle);

  // Sets up beforeunload API support for full-page PDF viewers.
  // TODO(crbug.com/1445746): Currently a no-op. Support the beforeunload API.
  void SetUpBeforeUnloadControl(
      mojo::PendingRemote<extensions::mime_handler::BeforeUnloadControl>
          before_unload_control_remote);

  // Stores stream info by embedder host info.
  std::map<EmbedderHostInfo, std::unique_ptr<StreamInfo>> stream_infos_;

  // Needed to avoid use-after-free when setting up beforeunload API support.
  base::WeakPtrFactory<PdfViewerStreamManager> weak_factory_{this};
};

}  // namespace pdf

#endif  // CHROME_BROWSER_PDF_PDF_VIEWER_STREAM_MANAGER_H_
