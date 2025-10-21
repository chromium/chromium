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
class GlicShareImageHandler {
 public:
  explicit GlicShareImageHandler(GlicKeyedService& service);
  ~GlicShareImageHandler();

  // Attempts to share an image with glic; triggered via context menu.
  void ShareContextImage(tabs::TabInterface* tab,
                         content::RenderFrameHost* render_frame_host,
                         const GURL& src_url);

 private:
  // Called once imaged data has been returned from the renderer.
  void ShareCapturedImage(tabs::TabHandle tab_handle,
                          const GURL& src_url,
                          const GURL& frame_url,
                          const url::Origin& frame_origin,
                          const std::vector<uint8_t>& thumbnail_data,
                          const gfx::Size& original_size,
                          const gfx::Size& downscaled_size,
                          const std::string& image_extension,
                          std::vector<lens::mojom::LatencyLogPtr> log_data);

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

  // This is used for communicating with the renderer to capture image context.
  std::unique_ptr<mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>>
      chrome_render_frame_remote_;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_CONTEXT_GLIC_SHARE_IMAGE_HANDLER_H_
