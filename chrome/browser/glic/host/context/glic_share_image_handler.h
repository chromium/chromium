// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARE_IMAGE_HANDLER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARE_IMAGE_HANDLER_H_

#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/lens/lens_metadata.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
}

namespace tabs {
class TabInterface;
}

namespace glic {

class GlicKeyedService;

// Manages the capturing of context images (i.e., images for which the user has
// opened the context menu), and sending to the web client as additional data.
class GlicShareImageHandler : public content::WebContentsObserver {
 public:
  explicit GlicShareImageHandler(GlicKeyedService& service);
  ~GlicShareImageHandler() override;

  // Attempts to share an image with glic; triggered via context menu.
  void ShareContextImage(tabs::TabInterface* tab,
                         content::RenderFrameHost* render_frame_host,
                         const GURL& src_url);

 private:
  // content::WebContentsObserver.
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

  // Called by TabInterface when the WebContents will be discarded.
  void OnWillDiscardContents(tabs::TabInterface* tab,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents);

  // Called when the tab is detached.
  void OnWillDetach(tabs::TabInterface* tab,
                    tabs::TabInterface::DetachReason reason);

  // Called once imaged data has been returned from the renderer.
  void OnReceivedImage(const std::vector<uint8_t>& thumbnail_data,
                       const gfx::Size& original_size,
                       const gfx::Size& downscaled_size,
                       const std::string& image_extension,
                       std::vector<lens::mojom::LatencyLogPtr> log_data);

  // Called once tab context has been fetched
  void OnReceivedTabContext(
      base::expected<glic::mojom::GetContextResultPtr,
                     page_content_annotations::FetchPageContextErrorDetails>
          result);

  // Attempt to display an error toast
  void MaybeShowErrorToast(tabs::TabInterface* tab);

  // Attempts to send the received context. The glic panel may not be ready,
  // however, and in that case, this function will begin polling for readiness
  // and will cancel after a timeout of 1 minute is exceeded.
  void SendAdditionalContextWhenReady();

  // Returns true if the glic client for the given tab is ready for context to
  // be sent.
  bool IsClientReady(tabs::TabInterface& tab);

  // Called when the end result of sharing is known. Sends context on success.
  void ShareComplete(ShareImageResult result);

  // A helper function to stop observation (since we want to do this before
  // calling Reset).
  void StopObservingNavigation();

  // Called whenever sharing is completed, successful or otherwise. Stops the
  // timer if it is running and clears state.
  void Reset();

  raw_ref<GlicKeyedService> service_;  // owns this

  bool is_share_in_progress_ = false;

  // TODO(b:448652827): Find another way to observe the outcome of ToggleUI.
  // For the moment, we will poll and these members are used for controlling
  // this process and sending the captured context when the panel is ready, if
  // possible.
  base::RepeatingTimer glic_panel_ready_timer_;
  base::TimeTicks glic_panel_open_time_;
  mojom::AdditionalContextPtr additional_context_;
  tabs::TabHandle tab_handle_;
  content::GlobalRenderFrameHostId render_frame_host_id_;
  GURL src_url_;
  GURL frame_url_;
  url::Origin frame_origin_;
  std::string mime_type_;
  std::vector<uint8_t> thumbnail_data_;
  base::CallbackListSubscription will_discard_web_contents_subscription_;
  base::CallbackListSubscription will_detach_subscription_;

  // This is used for communicating with the renderer to capture image context.
  std::unique_ptr<mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>>
      chrome_render_frame_remote_;

  base::WeakPtrFactory<GlicShareImageHandler> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARE_IMAGE_HANDLER_H_
