// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_TAB_CAPTURE_REGISTRY_H_
#define CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_TAB_CAPTURE_REGISTRY_H_

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/common/extensions/api/tab_capture.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_request_state.h"
#include "extensions/browser/browser_context_keyed_api_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_registry_observer.h"

namespace content {
class BrowserContext;
class WebContents;
}

namespace extensions {

namespace tab_capture = api::tab_capture;

class TabCaptureRegistry : public BrowserContextKeyedAPI,
                           public ExtensionRegistryObserver,
                           public MediaCaptureDevicesDispatcher::Observer {
 public:
  TabCaptureRegistry(const TabCaptureRegistry&) = delete;
  TabCaptureRegistry& operator=(const TabCaptureRegistry&) = delete;

  static TabCaptureRegistry* Get(content::BrowserContext* context);

  // Used by BrowserContextKeyedAPI.
  static BrowserContextKeyedAPIFactory<TabCaptureRegistry>*
      GetFactoryInstance();

  // List all pending, active and stopped capture requests.
  void GetCapturedTabs(const std::string& extension_id,
                       base::Value::List* capture_info_list) const;

  // Add a tab capture request to the registry when a stream is requested
  // through the API and create a randomly generated device id after user
  // initiated access to |source| for the |origin|. If capture is already
  // taking place for the same tab, this operation fails and returns an
  // empty string.
  // |target_contents|: the WebContents associated with the tab to be captured.
  // |extension_id|: the Extension initiating the request.
  // |is_anonymous| is true if GetCapturedTabs() should not list the captured
  // tab, and no status change events should be dispatched for it.
  // |caller_render_process_id|: the process ID associated with the
  // tab/extension that starts the capture.
  // |restrict_to_render_frame_id|: If populated, restricts the validity of the
  // capture request to a render frame with the specified ID. This may be empty
  // in the case of a Manifest V3 extension service worker calling
  // getMediaStreamId(), where it is designed to be consumed by another context
  // on the same origin (e.g., an offscreen document).
  std::string AddRequest(content::WebContents* target_contents,
                         const std::string& extension_id,
                         bool is_anonymous,
                         const GURL& origin,
                         content::DesktopMediaID source,
                         int caller_render_process_id,
                         std::optional<int> restrict_to_render_frame_id);

  // Called by MediaStreamDevicesController to verify the request before
  // creating the stream.  |render_process_id| and |render_frame_id| are used to
  // look-up a WebContents instance, which should match the |target_contents|
  // from the prior call to AddRequest().  In addition, a request is not
  // verified unless the |extension_id| also matches AND the request itself is
  // in the PENDING state.
  bool VerifyRequest(int render_process_id,
                     int render_frame_id,
                     const std::string& extension_id);

 private:
  friend class BrowserContextKeyedAPIFactory<TabCaptureRegistry>;
  class LiveRequest;

  explicit TabCaptureRegistry(content::BrowserContext* context);
  ~TabCaptureRegistry() override;

  // Used by BrowserContextKeyedAPI.
  static const char* service_name() {
    return "TabCaptureRegistry";
  }

  static const bool kServiceIsCreatedWithBrowserContext = false;
  static const bool kServiceRedirectedInIncognito = true;

  // ExtensionRegistryObserver implementation.
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const Extension* extension,
                           UnloadedExtensionReason reason) override;

  // MediaCaptureDevicesDispatcher::Observer implementation.
  void OnRequestUpdate(int target_render_process_id,
                       int target_render_frame_id,
                       blink::mojom::MediaStreamType stream_type,
                       const content::MediaRequestState state) override;

  // Send a StatusChanged event containing the current state of |request|.
  void DispatchStatusChangeEvent(const LiveRequest* request) const;

  // Look-up a LiveRequest associated with the given |target_contents| (or
  // the originally targetted RenderFrameHost), if any.
  LiveRequest* FindRequest(const content::WebContents* target_contents) const;
  LiveRequest* FindRequest(int target_render_process_id,
                           int target_render_frame_id) const;

  // Removes the |request| from |requests_|, thus causing its destruction.
  void KillRequest(LiveRequest* request);

  const raw_ptr<content::BrowserContext> browser_context_;
  std::vector<std::unique_ptr<LiveRequest>> requests_;

  base::ScopedObservation<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observation_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TAB_CAPTURE_TAB_CAPTURE_REGISTRY_H_
