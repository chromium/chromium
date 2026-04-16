// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PDF_MIME_HANDLER_STREAM_MANAGER_H_
#define CHROME_BROWSER_PDF_MIME_HANDLER_STREAM_MANAGER_H_

#include <map>
#include <memory>
#include <optional>

#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "extensions/browser/mime_handler/stream_info.h"

namespace content {
struct GlobalRenderFrameHostId;
class NavigationHandle;
class RenderFrameHost;
class SiteInstance;
class WebContents;
}  // namespace content

namespace extensions {
class StreamContainer;
}  // namespace extensions

namespace pdf {

// `MimeHandlerStreamManager` is used for PDF navigation. It tracks all
// PDF navigation events in a `content::WebContents`. It handles multiple PDF
// viewer instances in a single `content::WebContents`. It is responsible for:
// 1. Storing the `extensions::StreamContainer` PDF data.
// 2. Observing for the PDF frames either navigating or closing (including by
//    crashing). This is necessary to ensure that streams that aren't claimed
//    are not leaked, by deleting the stream if any of those events occur.
// 3. Observing for the RFH created by the PDF embedder RFH to load the PDF
//    extension URL.
// 4. Observing for the PDF content RFH to register the stream as a subresource
//    override for the final PDF commit navigation and to set up postMessage
//    support.
// `MimeHandlerStreamManager` is scoped to the `content::WebContents` it tracks,
// but it may also delete itself if all PDF streams are no longer used.
// `extensions::StreamContainer` objects are stored from
// `PluginResponseInterceptorURLLoaderThrottle::WillProcessResponse()` until
// the PDF viewer is no longer in use.
//
// Use `MimeHandlerStreamManager::Create()` to create an instance.
// Use `MimeHandlerStreamManager::FromWebContents()` to get an instance.
class MimeHandlerStreamManager
    : public content::WebContentsObserver,
      public content::WebContentsUserData<MimeHandlerStreamManager> {
 public:
  // A factory interface used to generate test PDF stream managers.
  class Factory {
   public:
    // If MimeHandlerStreamManager has a factory set, then
    // `MimeHandlerStreamManager::Create()` will automatically use
    // `CreateMimeHandlerStreamManager()` to create the PDF stream manager if
    // necessary for PDF navigations.
    virtual void CreateMimeHandlerStreamManager(
        content::WebContents* contents) = 0;

   protected:
    virtual ~Factory() = default;
  };

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
    content::FrameTreeNodeId frame_tree_node_id;
    content::GlobalRenderFrameHostId global_id;
  };

  // Creates a `MimeHandlerStreamManager` for `contents`, if one doesn't already
  // exist.
  static void Create(content::WebContents* contents);

  // Use `Create()` to create an instance instead.
  static void CreateForWebContents(content::WebContents*) = delete;

  MimeHandlerStreamManager(const MimeHandlerStreamManager&) = delete;
  MimeHandlerStreamManager& operator=(const MimeHandlerStreamManager&) = delete;
  ~MimeHandlerStreamManager() override;

  // Returns a pointer to the `MimeHandlerStreamManager` instance associated
  // with the `content::WebContents` of `render_frame_host`.
  static MimeHandlerStreamManager* FromRenderFrameHost(
      content::RenderFrameHost* render_frame_host);

  // Overrides factory for testing. Default (nullptr) value indicates regular
  // (non-test) environment.
  static void SetFactoryForTesting(Factory* factory);

  // Starts tracking a `StreamContainer` in an embedder FrameTreeNode, before
  // the embedder host commits. The `StreamContainer` is considered unclaimed
  // until the embedder host commits, at which point the `StreamContainer` is
  // tracked by both the frame tree node ID and the render frame host ID.
  // Replaces existing unclaimed entries with the same `frame_tree_node_id`.
  // This can occur if an embedder frame navigating to a PDF starts navigating
  // to another PDF URL before the original `StreamContainer` is claimed.
  void AddStreamContainer(
      content::FrameTreeNodeId frame_tree_node_id,
      const std::string& internal_id,
      std::unique_ptr<extensions::StreamContainer> stream_container);

  // Returns a pointer to a stream container that `embedder_host` has claimed or
  // nullptr if `embedder_host` hasn't claimed any stream containers.
  base::WeakPtr<extensions::StreamContainer> GetStreamContainer(
      content::RenderFrameHost* embedder_host);

  // Returns true if `render_frame_host` is an extension host for a PDF. During
  // a PDF load, the initial RFH for the extension frame commits to the
  // about:blank URL. Another RFH will then be chosen to host the extension.
  // This returns true for both hosts. Depending on what navigation step the
  // frame is on, callers can also check the last committed origin to
  // differentiate between the hosts.
  bool IsPdfExtensionHost(
      const content::RenderFrameHost* render_frame_host) const;

  // Returns true if `frame_tree_node_id` is the frame tree node ID for the PDF
  // extension frame under `embedder_host`, false otherwise.
  bool IsPdfExtensionFrameTreeNodeId(
      const content::RenderFrameHost* embedder_host,
      content::FrameTreeNodeId frame_tree_node_id) const;

  // Returns true if `embedder_host` has a PDF extension frame and it has
  // already finished its navigation, false otherwise.
  bool DidPdfExtensionFinishNavigation(
      const content::RenderFrameHost* embedder_host) const;

  // Returns true if `render_frame_host` is a content host for a PDF. During a
  // PDF load, the initial RFH for the content frame attempts to navigate to the
  // stream URL. Another RFH will then be chosen to host the content frame. This
  // returns true for both hosts. Depending on what navigation step the frame is
  // on, callers can also check the last committed URL to differentiate between
  // the hosts.
  bool IsPdfContentHost(
      const content::RenderFrameHost* render_frame_host) const;

  // Returns true if `frame_tree_node_id` is the frame tree node ID for the PDF
  // content frame under `embedder_host`, false otherwise.
  bool IsPdfContentFrameTreeNodeId(
      const content::RenderFrameHost* embedder_host,
      content::FrameTreeNodeId frame_tree_node_id) const;

  // Returns true if `embedder_host` has a PDF content frame and it has already
  // finished its navigation, false otherwise.
  bool DidPdfContentNavigate(
      const content::RenderFrameHost* embedder_host) const;

  // Returns whether the PDF plugin should handle save events.
  bool PluginCanSave(const content::RenderFrameHost* embedder_host) const;

  // Set whether the PDF plugin should handle save events.
  void SetPluginCanSave(content::RenderFrameHost* embedder_host,
                        bool plugin_can_save);

  // Returns whether there's an unclaimed stream info with the default embedder
  // host info.
  bool ContainsUnclaimedStreamInfo(
      content::FrameTreeNodeId frame_tree_node_id) const;

  // Deletes the unclaimed stream info associated with `frame_tree_node_id`, and
  // deletes `this` if there are no remaining stream infos. Callers must ensure
  // such a stream info exists before calling this, otherwise crashes.
  void DeleteUnclaimedStreamInfo(content::FrameTreeNodeId frame_tree_node_id);

  // WebContentsObserver overrides.
  void RenderFrameDeleted(content::RenderFrameHost* render_frame_host) override;
  void RenderFrameHostChanged(content::RenderFrameHost* old_host,
                              content::RenderFrameHost* new_host) override;
  void FrameDeleted(content::FrameTreeNodeId frame_tree_node_id) override;
  void DidStartNavigation(
      content::NavigationHandle* navigation_handle) override;
  void ReadyToCommitNavigation(
      content::NavigationHandle* navigation_handle) override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // For testing only. Mark an unclaimed stream info with the same frame tree
  // node ID as `embedder_host` as claimed by `embedder_host`. Callers must
  // ensure such a stream info exists before calling this.
  void ClaimStreamInfoForTesting(content::RenderFrameHost* embedder_host);

  // For testing only. Set `embedder_host`'s extension frame tree node ID as
  // `frame_tree_node_id`. This is needed to listen for extension host deletion.
  // Callers must ensure that `embedder_host` has a claimed stream info.
  void SetExtensionFrameTreeNodeIdForTesting(
      content::RenderFrameHost* embedder_host,
      content::FrameTreeNodeId frame_tree_node_id);

  // For testing only. Set `embedder_host`'s content frame tree node ID as
  // `frame_tree_node_id`. This is needed to listen for content host deletion.
  // Callers must ensure that `embedder_host` has a claimed stream info.
  void SetContentFrameTreeNodeIdForTesting(
      content::RenderFrameHost* embedder_host,
      content::FrameTreeNodeId frame_tree_node_id);

 protected:
  // Use `Create()` to create an instance instead.
  explicit MimeHandlerStreamManager(content::WebContents* contents);

  // Returns the stream info claimed by `embedder_host`, or nullptr if there's
  // no existing stream.
  extensions::StreamInfo* GetClaimedStreamInfo(
      const content::RenderFrameHost* embedder_host);
  const extensions::StreamInfo* GetClaimedStreamInfo(
      const content::RenderFrameHost* embedder_host) const;

  // Returns the stream info for a PDF content navigation.
  extensions::StreamInfo* GetClaimedStreamInfoFromPdfContentNavigation(
      content::NavigationHandle* navigation_handle);

  // Navigates the FrameTreeNode with ID `extension_host_frame_tree_node_id` to
  // the PDF extension URL. Marks the PDF extension as navigated in
  // `stream_info`, which must be non-null. `source_site_instance` should be the
  // `content::SiteInstance` of the PDF embedder frame that will be initiating
  // the navigation.
  //
  // Subclasses may override this for use in callbacks. If so, `global_id`,
  // which is the ID for the intermediate about:blank host for the PDF extension
  // frame, can be used to get the other parameters safely.
  virtual void NavigateToPdfExtensionUrl(
      content::FrameTreeNodeId extension_host_frame_tree_node_id,
      extensions::StreamInfo* stream_info,
      content::SiteInstance* source_site_instance,
      content::GlobalRenderFrameHostId global_id);

 private:
  friend class content::WebContentsUserData<MimeHandlerStreamManager>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

  // Mark an unclaimed stream info with the same frame tree node ID as
  // `embedder_host` as claimed by `embedder_host`. Returns a pointer to the
  // claimed stream info. Callers must ensure such a stream info exists with
  // `ContainsUnclaimedStreamInfo()` before calling this.
  extensions::StreamInfo* ClaimStreamInfo(
      content::RenderFrameHost* embedder_host);

  // Deletes the claimed stream info associated with `embedder_host`, and
  // deletes `this` if there are no remaining stream infos.
  void DeleteClaimedStreamInfo(content::RenderFrameHost* embedder_host);

  // Intended to be called when a RenderFrameHost in the observed
  // `content::WebContents` is replaced or deleted. If `render_frame_host` is a
  // deleted PDF extension host, then delete the stream. Deletes `this` if there
  // are no remaining streams. Returns true if the stream was deleted, false
  // otherwise.
  [[nodiscard]] bool MaybeDeleteStreamOnPdfExtensionHostChanged(
      content::RenderFrameHost* old_host);

  // Same as `MaybeDeleteStreamOnPdfExtensionHostChanged()`, but for the content
  // host.
  [[nodiscard]] bool MaybeDeleteStreamOnPdfContentHostChanged(
      content::RenderFrameHost* old_host);

  // Intended to be called during the PDF content frame's
  // `ReadyToCommitNavigation()` event. Registers navigations occurring in a PDF
  // content frame as a subresource.
  bool MaybeRegisterPdfSubresourceOverride(
      content::NavigationHandle* navigation_handle);

  // Intended to be called during the PDF content frame's 'DidFinishNavigation'.
  // Sets up postMessage communication between the embedder frame and the PDF
  // extension frame after the PDF has finished loading.
  bool MaybeSetUpPostMessage(content::NavigationHandle* navigation_handle);

  // During the PDF content frame navigation, set the related PDF stream's
  // content host frame tree node ID.
  void SetStreamContentHostFrameTreeNodeId(
      content::NavigationHandle* navigation_handle);

  // Stores stream info by embedder host info.
  std::map<EmbedderHostInfo, std::unique_ptr<extensions::StreamInfo>>
      stream_infos_;
};

}  // namespace pdf

#endif  // CHROME_BROWSER_PDF_MIME_HANDLER_STREAM_MANAGER_H_
