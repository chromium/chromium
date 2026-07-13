// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARE_IMAGE_HANDLER_H_
#define CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARE_IMAGE_HANDLER_H_

#include <string>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/ui/tabs/page_context_eligibility_helper.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/lens/lens_metadata.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/gfx/geometry/size.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class RenderFrameHost;
class WebContents;
}  // namespace content

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
  friend class GlicShareImageHandlerTest;

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

  // Attempt to display an error toast
  void MaybeShowErrorToast(tabs::TabInterface* tab);

  // Called if the invoke API hits a failure. This completes the share process
  // and causes metrics to be logged.
  void OnInvokeError(GlicInvokeError error);

  // Called when the end result of sharing is known. Sends context on success.
  void ShareComplete(ShareImageResult result);

  // A helper function to stop observation (since we want to do this before
  // calling Reset).
  void StopObservingNavigation();

  // Returns true if clipboard policy checks are required for the current state.
  // If `size` is not provided, we will use the maximum image size.
  bool AreClipboardPolicyChecksRequired(std::optional<size_t> size);

  // Starts observing navigation if policy checks are required.
  void MaybeStartObservingNavigation(tabs::TabInterface* tab);

  // Starts observing page context eligibility changes. Returns false if the
  // context is ineligible or eligibility cannot be determined.
  bool MaybeStartObservingEligibility(tabs::TabInterface* tab);

  // Called when eligibility changes.
  void OnPageContextEligibilityChanged(
      optimization_guide::PageContextEligibilityStatus eligibility);

  // Called whenever sharing is completed, successful or otherwise. Stops the
  // timer if it is running and clears state.
  void Reset();

  raw_ref<GlicKeyedService> service_;  // owns this

  bool is_share_in_progress_ = false;

  tabs::TabHandle tab_handle_;
  content::GlobalRenderFrameHostId render_frame_host_id_;
  GURL src_url_;
  GURL frame_url_;
  url::Origin frame_origin_;
  base::CallbackListSubscription will_discard_web_contents_subscription_;
  base::CallbackListSubscription will_detach_subscription_;
  base::CallbackListSubscription eligibility_subscription_;

  // This is used for communicating with the renderer to capture image context.
  std::unique_ptr<mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>>
      chrome_render_frame_remote_;

  base::WeakPtrFactory<GlicShareImageHandler> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARE_IMAGE_HANDLER_H_
